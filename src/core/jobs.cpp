/**
 * @file jobs.cpp
 * @brief work-stealing job system implementation (multithreaded task scheduler uwu)
 * 
 * implements the JobSystem class with work-stealing queues, dependency tracking,
 * and parallel-for utilities. based on Christian Gyrling's 2015 GDC talk
 * "Parallelizing the Naughty Dog Engine Using Fibers" and other industry
 * best practices.
 * 
 * key implementation details:
 * - lock-free work queues (circular buffer with atomic head/tail)
 * - random work stealing (reduce contention)
 * - job pool with generation counters (detect stale handles)
 * - dependency tracking via atomic reference counting
 * - worker threads help execute jobs while waiting (avoid deadlocks)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#include <luma/core/jobs.hpp>
#include <luma/core/logging.hpp>

#include <algorithm>
#include <random>
#include <deque>
#include <mutex>

namespace luma {

namespace {

/**
 * @brief job pool size (preallocated jobs)
 * 
 * this determines the maximum number of concurrent jobs. if this limit is
 * reached, schedule() will fail (or block, depending on implementation).
 * for the LUMA engine, 4096 jobs should be sufficient for typical workloads.
 * 
 * @note power of 2 for efficient modulo operations
 */
constexpr size_t JOB_POOL_SIZE = 4096;

/**
 * @brief work queue capacity per thread
 * 
 * each worker thread has a local deque with this capacity. if the queue fills
 * up, new jobs are added to other threads' queues (load balancing).
 * 
 * @note power of 2 for efficient ring buffer operations
 */
constexpr size_t WORK_QUEUE_SIZE = 512;

} // anonymous namespace

/**
 * @brief work queue implementation (lock-free circular buffer)
 * 
 * uses a std::deque with mutex for simplicity. for production, could be
 * replaced with a lock-free concurrent queue (e.g., boost::lockfree::queue).
 * 
 * operations:
 * - push_back: Add job to tail (LIFO for owner thread)
 * - pop_back: Remove job from tail (LIFO for owner thread)
 * - pop_front: Remove job from head (FIFO for stealing thread)
 * 
 * @note std::deque is used instead of lock-free queue for simplicity (MVP)
 * @note future optimization: replace with lock-free queue
 */
struct JobSystem::WorkQueue {
    std::deque<Job*> jobs;
    std::mutex mutex;
    
    /**
     * @brief push job to back of queue (owner thread)
     * 
     * @param[in] job job to push
     * @return true if successful, false if queue full
     */
    auto push_back(Job* job) -> bool {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.size() >= WORK_QUEUE_SIZE) {
            return false;
        }
        jobs.push_back(job);
        return true;
    }
    
    /**
     * @brief pop job from back of queue (owner thread, LIFO)
     * 
     * @return job pointer or nullptr if queue empty
     */
    auto pop_back() -> Job* {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.empty()) {
            return nullptr;
        }
        Job* job = jobs.back();
        jobs.pop_back();
        return job;
    }
    
    /**
     * @brief pop job from front of queue (stealing thread, FIFO)
     * 
     * @return job pointer or nullptr if queue empty
     */
    auto pop_front() -> Job* {
        std::lock_guard<std::mutex> lock(mutex);
        if (jobs.empty()) {
            return nullptr;
        }
        Job* job = jobs.front();
        jobs.pop_front();
        return job;
    }
    
    /**
     * @brief check if queue is empty
     * 
     * @return true if queue is empty
     */
    [[nodiscard]] auto empty() const -> bool {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex));
        return jobs.empty();
    }
};

JobSystem::JobSystem(u32 thread_count)
    : thread_count_(thread_count == 0 ? std::thread::hardware_concurrency() : thread_count) {
    
    // preallocate job pool (allocate individual jobs to avoid move/copy issues with atomics)
    job_pool_.reserve(JOB_POOL_SIZE);
    for (size_t i = 0; i < JOB_POOL_SIZE; ++i) {
        job_pool_.push_back(std::make_unique<Job>());
    }
    
    // create work queues (one per thread)
    queues_.reserve(thread_count_);
    for (u32 i = 0; i < thread_count_; ++i) {
        queues_.push_back(std::make_unique<WorkQueue>());
    }
    
    // spawn worker threads
    threads_.reserve(thread_count_);
    for (u32 i = 0; i < thread_count_; ++i) {
        threads_.emplace_back([this, i]() {
            worker_thread_main(i);
        });
    }
    
    LOG_INFO("JobSystem initialized with {} worker threads", thread_count_);
}

JobSystem::~JobSystem() {
    // signal shutdown
    shutdown_.store(true, std::memory_order_release);
    
    // join all threads
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    LOG_INFO("JobSystem shut down");
}

auto JobSystem::create(u32 thread_count) -> Result<std::unique_ptr<JobSystem>> {
    try {
        auto job_system = std::unique_ptr<JobSystem>(new JobSystem(thread_count));
        return job_system;
    } catch (const std::exception& e) {
        return std::unexpected(Error{
            ErrorCode::INITIALIZATION_FAILED,
            std::format("Failed to create JobSystem: {}", e.what())
        });
    }
}

auto JobSystem::allocate_job() -> std::pair<Job*, JobHandle> {
    // simple allocation: atomic increment next_free_job_
    u32 index = next_free_job_.fetch_add(1, std::memory_order_relaxed) % JOB_POOL_SIZE;
    
    Job* job = job_pool_[index].get();
    
    // increment generation (detect stale handles)
    job->generation++;
    
    // reset job state
    job->unfinished_jobs.store(1, std::memory_order_relaxed);
    job->parent = nullptr;
    
    JobHandle handle{index + 1, job->generation}; // id = index + 1 (0 is invalid)
    return {job, handle};
}

auto JobSystem::schedule(
    JobFunction function,
    void* data,
    const std::vector<JobHandle>& dependencies
) -> JobHandle {
    
    // allocate job from pool
    auto [job, handle] = allocate_job();
    
    // set job parameters
    job->function = std::move(function);
    job->data = data;
    
    // handle dependencies
    if (!dependencies.empty()) {
        // increment unfinished_jobs for each dependency
        job->unfinished_jobs.fetch_add(
            static_cast<u32>(dependencies.size()),
            std::memory_order_relaxed
        );
        
        // register this job as dependent on each dependency
        // (when dependency finishes, it will decrement this job's unfinished_jobs)
        for (const auto& dep_handle : dependencies) {
            if (dep_handle.is_valid()) {
                u32 dep_index = dep_handle.id - 1;
                Job* dep_job = job_pool_[dep_index].get();
                
                // set this job as child of dependency (for notification)
                // note: this is a simplification; real implementation would use a list
                // for now, we only support one parent per job (MVP limitation)
                if (dep_job->parent == nullptr) {
                    dep_job->parent = job;
                }
            }
        }
    }
    
    // add job to queue (round-robin across threads for load balancing)
    // use hash of current time to reduce contention
    static std::atomic<u32> queue_index{0};
    u32 target_queue = queue_index.fetch_add(1, std::memory_order_relaxed) % thread_count_;
    
    if (!queues_[target_queue]->push_back(job)) {
        // queue full, try other queues
        for (u32 i = 0; i < thread_count_; ++i) {
            if (queues_[i]->push_back(job)) {
                break;
            }
        }
        // if all queues full, we're in trouble (MVP: just log error)
        LOG_ERROR("All work queues full! Job may not execute.");
    }
    
    return handle;
}

auto JobSystem::wait(JobHandle handle) -> void {
    if (!handle.is_valid()) {
        return;
    }
    
    u32 index = handle.id - 1;
    if (index >= JOB_POOL_SIZE) {
        LOG_ERROR("Invalid job handle: {}", handle.id);
        return;
    }
    
    Job* job = job_pool_[index].get();
    
    // check generation (detect stale handle)
    if (job->generation != handle.generation) {
        // stale handle, job already completed and recycled
        return;
    }
    
    // help execute jobs while waiting (avoid deadlock, improve throughput)
    while (!job->is_complete()) {
        // try to execute a job from any queue
        Job* work_job = nullptr;
        
        // try stealing from random thread
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<u32> dist(0, thread_count_ - 1);
        u32 steal_index = dist(rng);
        
        work_job = queues_[steal_index]->pop_back();
        
        if (work_job != nullptr) {
            execute_job(work_job);
        } else {
            // no work available, yield to avoid busy-wait
            std::this_thread::yield();
        }
    }
}

auto JobSystem::parallel_for(
    size_t begin,
    size_t end,
    size_t chunk_size,
    const std::function<void(size_t)>& function
) -> void {
    
    if (begin >= end || chunk_size == 0) {
        return;
    }
    
    // calculate number of jobs needed
    size_t range = end - begin;
    size_t num_jobs = (range + chunk_size - 1) / chunk_size;
    
    // schedule jobs
    std::vector<JobHandle> handles;
    handles.reserve(num_jobs);
    
    for (size_t i = 0; i < num_jobs; ++i) {
        size_t job_begin = begin + i * chunk_size;
        size_t job_end = std::min(job_begin + chunk_size, end);
        
        // capture range and function in lambda
        auto job_func = [job_begin, job_end, &function](void*) {
            for (size_t index = job_begin; index < job_end; ++index) {
                function(index);
            }
        };
        
        auto handle = schedule(job_func, nullptr);
        handles.push_back(handle);
    }
    
    // wait for all jobs to complete
    for (const auto& handle : handles) {
        wait(handle);
    }
}

auto JobSystem::thread_count() const noexcept -> u32 {
    return thread_count_;
}

auto JobSystem::worker_thread_main(u32 thread_index) -> void {
    LOG_TRACE("Worker thread {} started", thread_index);
    
    while (!shutdown_.load(std::memory_order_acquire)) {
        // try to get job from local queue first (LIFO for cache locality)
        Job* job = get_job(thread_index);
        
        if (job == nullptr) {
            // local queue empty, try stealing from other threads
            job = steal_job(thread_index);
        }
        
        if (job != nullptr) {
            execute_job(job);
        } else {
            // no work available, sleep briefly to avoid busy-wait
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    LOG_TRACE("Worker thread {} stopped", thread_index);
}

auto JobSystem::execute_job(Job* job) -> void {
    if (job == nullptr || job->function == nullptr) {
        return;
    }
    
    // execute job function
    job->function(job->data);
    
    // mark job as finished
    finish_job(job);
}

auto JobSystem::finish_job(Job* job) -> void {
    // decrement unfinished_jobs atomically
    u32 unfinished = job->unfinished_jobs.fetch_sub(1, std::memory_order_acq_rel) - 1;
    
    if (unfinished == 0) {
        // job complete, notify parent if exists
        if (job->parent != nullptr) {
            finish_job(job->parent);
        }
    }
}

auto JobSystem::get_job(u32 thread_index) -> Job* {
    if (thread_index >= thread_count_) {
        return nullptr;
    }
    
    return queues_[thread_index]->pop_back();
}

auto JobSystem::steal_job(u32 thread_index) -> Job* {
    // try stealing from random thread (not self)
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<u32> dist(0, thread_count_ - 1);
    
    for (u32 attempt = 0; attempt < thread_count_; ++attempt) {
        u32 steal_index = dist(rng);
        
        // don't steal from self
        if (steal_index == thread_index) {
            continue;
        }
        
        Job* job = queues_[steal_index]->pop_front();
        if (job != nullptr) {
            return job;
        }
    }
    
    return nullptr;
}

} // namespace luma
