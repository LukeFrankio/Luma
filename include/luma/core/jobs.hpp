/**
 * @file jobs.hpp
 * @brief work-stealing job system for parallel task execution uwu
 * 
 * this file implements a lock-free work-stealing job system based on Christian
 * Gyrling's 2015 GDC talk and Naughty Dog's fiber-based task system. jobs are
 * pure functions with explicit dependencies forming a directed acyclic graph
 * (DAG). the job system dynamically distributes work across all CPU threads
 * using work-stealing queues for load balancing.
 * 
 * key features:
 * - work-stealing deques (lock-free when possible, fallback to mutex)
 * - dependency tracking via reference counting (atomic operations)
 * - job handles with generation counters (detect stale references)
 * - dynamic thread pool (uses all available CPU cores by default)
 * - jobs are pure functions (no shared mutable state)
 * 
 * design philosophy:
 * - functional programming: jobs are pure functions with explicit inputs/outputs
 * - lock-free where possible: minimize contention, maximize throughput
 * - cache-friendly: jobs stored contiguously, dependencies tracked efficiently
 * - composable: jobs can spawn child jobs, wait on dependencies
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note uses C++26 features (latest standard, even beta features ok!)
 * @note compiled with GCC 15+ (latest version preferred)
 * @warning requires std::atomic, std::thread (C++11 minimum)
 * 
 * example usage (simple job):
 * @code
 * auto job_system = JobSystem::create();
 * 
 * // schedule a pure function
 * auto handle = job_system->schedule([](void* data) {
 *     auto value = static_cast<int*>(data);
 *     LOG_INFO("Processing value: {}", *value);
 * }, &my_value);
 * 
 * // wait for completion
 * job_system->wait(handle);
 * @endcode
 * 
 * example usage (dependencies):
 * @code
 * auto job_system = JobSystem::create();
 * 
 * // job A (runs first)
 * auto handle_a = job_system->schedule([](void* data) {
 *     LOG_INFO("Job A executing");
 * }, nullptr);
 * 
 * // job B (depends on A)
 * auto handle_b = job_system->schedule([](void* data) {
 *     LOG_INFO("Job B executing (after A)");
 * }, nullptr, {handle_a});
 * 
 * // wait for B (implicitly waits for A)
 * job_system->wait(handle_b);
 * @endcode
 * 
 * example usage (parallel for):
 * @code
 * auto job_system = JobSystem::create();
 * 
 * // process array elements in parallel
 * std::vector<int> data(1000);
 * job_system->parallel_for(0, data.size(), 64, [&data](size_t index) {
 *     data[index] = process(data[index]);
 * });
 * @endcode
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/core/math.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace luma {

/**
 * @brief function signature for job execution
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * jobs should be pure functions (no side effects except through data pointer).
 * the data pointer is provided by the user when scheduling the job.
 * 
 * @param data user-provided data pointer (can be nullptr)
 * 
 * @note prefer passing data via pointer over capturing in lambda (better cache locality)
 * @note ensure data lifetime exceeds job lifetime (no dangling pointers!)
 */
using JobFunction = std::function<void(void*)>;

/**
 * @brief handle to a scheduled job (with generation counter for safety)
 * 
 * job handles allow waiting on job completion and detecting stale references.
 * the generation counter increments on job completion/deletion, preventing
 * use-after-free bugs when jobs are recycled.
 * 
 * @note handles are lightweight (8 bytes), copyable, and comparable
 * @note invalid handles have id = 0, generation = 0
 */
struct JobHandle {
    u32 id{0};         ///< index into job pool (0 = invalid)
    u32 generation{0}; ///< generation counter (increments on reuse)
    
    /**
     * @brief check if handle is valid
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if handle is valid (id != 0), false otherwise
     */
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return id != 0;
    }
    
    /**
     * @brief compare two handles for equality
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param other handle to compare against
     * @return true if handles are equal (same id and generation)
     */
    [[nodiscard]] constexpr auto operator==(const JobHandle& other) const noexcept -> bool {
        return id == other.id && generation == other.generation;
    }
    
    /**
     * @brief compare two handles for inequality
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param other handle to compare against
     * @return true if handles are not equal
     */
    [[nodiscard]] constexpr auto operator!=(const JobHandle& other) const noexcept -> bool {
        return !(*this == other);
    }
};

/**
 * @brief internal job structure (opaque to users)
 * 
 * jobs are scheduled by the job system and stored in a pool. users interact
 * with jobs via JobHandle, not directly. jobs track their function, data,
 * dependencies, and completion state.
 * 
 * @note jobs are reused after completion (generation counter incremented)
 * @note jobs use atomic reference counting for thread-safe dependency tracking
 */
struct Job {
    JobFunction function;                     ///< function to execute
    void* data{nullptr};                      ///< user-provided data pointer
    std::atomic<u32> unfinished_jobs{1};      ///< reference count (starts at 1, decrements on completion)
    Job* parent{nullptr};                     ///< parent job (for child job dependency tracking)
    u32 generation{0};                        ///< generation counter (increments on reuse)
    
    /**
     * @brief check if job is complete
     * 
     * ✨ PURE FUNCTION ✨ (atomic read is side-effect-free)
     * 
     * @return true if job has finished execution
     */
    [[nodiscard]] auto is_complete() const noexcept -> bool {
        return unfinished_jobs.load(std::memory_order_acquire) == 0;
    }
};

/**
 * @brief work-stealing job system (multithreaded task scheduler)
 * 
 * the job system manages a pool of worker threads that execute jobs from
 * work-stealing deques. when a thread's queue is empty, it steals work from
 * other threads (load balancing). jobs can have dependencies (expressed via
 * reference counting) and can spawn child jobs.
 * 
 * design:
 * - one deque per thread (minimize contention)
 * - work-stealing from random thread's tail (LIFO for owner, FIFO for thief)
 * - jobs stored in contiguous pool (cache-friendly, generation counters)
 * - dependency tracking via atomic reference counting
 * 
 * @note singleton pattern (one job system per application)
 * @note thread-safe (all methods are lock-free or use minimal locking)
 * @note automatically shuts down on destruction (joins all threads)
 * 
 * @see JobHandle
 * @see Job
 */
class JobSystem {
public:
    /**
     * @brief create job system with specified thread count
     * 
     * ⚠️ IMPURE FUNCTION (allocates threads, starts background execution)
     * 
     * creates a job system with the specified number of worker threads. if
     * thread_count is 0, uses std::thread::hardware_concurrency() (all cores).
     * 
     * @param[in] thread_count number of worker threads (0 = auto-detect)
     * 
     * @return Result containing JobSystem unique_ptr or error
     * @retval OK if successful
     * @retval INITIALIZATION_FAILED if thread creation failed
     * 
     * @note prefer using all available cores (thread_count = 0)
     * @note more threads than cores may hurt performance (context switching)
     * 
     * @complexity O(n) where n = thread_count (spawns threads)
     * 
     * example:
     * @code
     * auto job_system = JobSystem::create();
     * if (!job_system) {
     *     LOG_ERROR("Failed to create job system: {}", job_system.error().message());
     *     return;
     * }
     * // job system is now ready to schedule jobs
     * @endcode
     */
    [[nodiscard]] static auto create(u32 thread_count = 0) -> Result<std::unique_ptr<JobSystem>>;
    
    /**
     * @brief destructor (joins all worker threads)
     * 
     * ⚠️ IMPURE FUNCTION (blocks until all threads finish)
     * 
     * waits for all in-flight jobs to complete, then joins all worker threads.
     * this ensures no jobs are executing when the job system is destroyed.
     * 
     * @warning blocking operation (may take time if jobs are still running)
     * @note automatically called when JobSystem goes out of scope
     */
    ~JobSystem();
    
    // non-copyable, non-movable (singleton pattern)
    JobSystem(const JobSystem&) = delete;
    auto operator=(const JobSystem&) -> JobSystem& = delete;
    JobSystem(JobSystem&&) = delete;
    auto operator=(JobSystem&&) -> JobSystem& = delete;
    
    /**
     * @brief schedule a job with optional dependencies
     * 
     * ⚠️ IMPURE FUNCTION (adds job to queue, modifies internal state)
     * 
     * schedules a job for execution on a worker thread. the job will not run
     * until all dependencies have completed. dependencies are expressed as
     * JobHandles from previous schedule() calls.
     * 
     * @param[in] function job function to execute
     * @param[in] data user-provided data pointer (passed to function)
     * @param[in] dependencies list of jobs that must complete first
     * 
     * @return JobHandle to the scheduled job
     * 
     * @pre function must be valid (not null)
     * @pre all dependency handles must be valid
     * @post job is added to queue and will execute when dependencies satisfied
     * 
     * @note if no dependencies, job may start immediately
     * @note data pointer must remain valid until job completes
     * @note job function should be pure (no shared mutable state)
     * 
     * @complexity O(1) amortized (queue push)
     * 
     * example:
     * @code
     * auto handle = job_system->schedule([](void* data) {
     *     LOG_INFO("Job executing!");
     * }, nullptr);
     * @endcode
     * 
     * example with dependencies:
     * @code
     * auto handle_a = job_system->schedule([](void*) { // job A
     * }, nullptr);
     * auto handle_b = job_system->schedule([](void*) { // job B
     * }, nullptr);
     * auto handle_c = job_system->schedule([](void*) { // job C
     * }, nullptr, {handle_a, handle_b});
     * // job C runs after both A and B complete
     * @endcode
     */
    [[nodiscard]] auto schedule(
        JobFunction function,
        void* data = nullptr,
        const std::vector<JobHandle>& dependencies = {}
    ) -> JobHandle;
    
    /**
     * @brief wait for a job to complete
     * 
     * ⚠️ IMPURE FUNCTION (blocks current thread, executes jobs while waiting)
     * 
     * blocks the calling thread until the specified job completes. while waiting,
     * the thread helps execute jobs from the queue (work stealing). this ensures
     * forward progress and avoids deadlocks.
     * 
     * @param[in] handle handle to job to wait for
     * 
     * @pre handle must be valid
     * @post job has completed execution when function returns
     * 
     * @note thread does not idle (executes other jobs while waiting)
     * @note safe to call from main thread or worker threads
     * @warning do not wait on invalid handles (undefined behavior)
     * 
     * @complexity O(n) where n = jobs executed while waiting
     * 
     * example:
     * @code
     * auto handle = job_system->schedule([](void*) {
     *     // work here
     * }, nullptr);
     * job_system->wait(handle);
     * // job is guaranteed to be complete here
     * @endcode
     */
    auto wait(JobHandle handle) -> void;
    
    /**
     * @brief parallel for loop (data-parallel execution)
     * 
     * ⚠️ IMPURE FUNCTION (schedules jobs, blocks until completion)
     * 
     * executes a function for each index in range [begin, end), splitting work
     * across multiple jobs with the specified chunk size. this is equivalent to:
     * 
     * ```cpp
     * for (size_t i = begin; i < end; ++i) {
     *     function(i);
     * }
     * ```
     * 
     * but executed in parallel across worker threads.
     * 
     * @param[in] begin start index (inclusive)
     * @param[in] end end index (exclusive)
     * @param[in] chunk_size number of iterations per job
     * @param[in] function function to execute for each index
     * 
     * @pre begin < end
     * @pre chunk_size > 0
     * @pre function must be thread-safe (no shared mutable state)
     * @post function has been called for all indices when returns
     * 
     * @note chunk_size affects granularity (too small = overhead, too large = load imbalance)
     * @note prefer chunk_size = 64-256 for most workloads
     * @note blocks until all iterations complete
     * 
     * @complexity O(n / chunk_size) jobs scheduled, O(n) total work
     * 
     * example:
     * @code
     * std::vector<int> data(1000);
     * job_system->parallel_for(0, data.size(), 64, [&data](size_t i) {
     *     data[i] = compute(i);  // process each element
     * });
     * // all elements processed when this returns
     * @endcode
     */
    auto parallel_for(
        size_t begin,
        size_t end,
        size_t chunk_size,
        const std::function<void(size_t)>& function
    ) -> void;
    
    /**
     * @brief get number of worker threads
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return number of worker threads in the pool
     */
    [[nodiscard]] auto thread_count() const noexcept -> u32;
    
private:
    /**
     * @brief private constructor (use create() factory function)
     * 
     * @param[in] thread_count number of worker threads to spawn
     */
    explicit JobSystem(u32 thread_count);
    
    /**
     * @brief worker thread main loop
     * 
     * ⚠️ IMPURE FUNCTION (infinite loop, executes jobs)
     * 
     * worker threads run this function continuously, executing jobs from their
     * local deque and stealing work from other threads when idle.
     * 
     * @param[in] thread_index index of this thread (for work stealing)
     */
    auto worker_thread_main(u32 thread_index) -> void;
    
    /**
     * @brief allocate a job from the pool
     * 
     * ⚠️ IMPURE FUNCTION (modifies pool state)
     * 
     * @return pointer to allocated job and its handle
     */
    auto allocate_job() -> std::pair<Job*, JobHandle>;
    
    /**
     * @brief execute a job and decrement its reference count
     * 
     * ⚠️ IMPURE FUNCTION (executes job function, modifies state)
     * 
     * @param[in] job job to execute
     */
    auto execute_job(Job* job) -> void;
    
    /**
     * @brief finish a job (decrement reference count, notify parent)
     * 
     * ⚠️ IMPURE FUNCTION (atomic operations, modifies state)
     * 
     * @param[in] job job that has finished
     */
    auto finish_job(Job* job) -> void;
    
    /**
     * @brief try to get a job from the local queue
     * 
     * ⚠️ IMPURE FUNCTION (modifies queue)
     * 
     * @param[in] thread_index index of this thread
     * @return pointer to job or nullptr if queue empty
     */
    auto get_job(u32 thread_index) -> Job*;
    
    /**
     * @brief try to steal a job from another thread's queue
     * 
     * ⚠️ IMPURE FUNCTION (modifies other thread's queue)
     * 
     * @param[in] thread_index index of this thread (to avoid stealing from self)
     * @return pointer to stolen job or nullptr if no work available
     */
    auto steal_job(u32 thread_index) -> Job*;
    
    // forward declare internal implementation types
    struct WorkQueue;
    
    std::vector<std::thread> threads_;                      ///< worker threads
    std::vector<std::unique_ptr<WorkQueue>> queues_;        ///< per-thread work queues
    std::vector<std::unique_ptr<Job>> job_pool_;            ///< job storage pool (pointers to avoid atomic copy issues)
    std::atomic<u32> next_free_job_{0};                     ///< next free job index
    std::atomic<bool> shutdown_{false};                     ///< shutdown flag
    u32 thread_count_{0};                                   ///< number of worker threads
};

} // namespace luma
