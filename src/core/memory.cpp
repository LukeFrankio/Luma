/**
 * @file memory.cpp
 * @brief custom memory allocators implementation (fast allocation for specific patterns uwu)
 * 
 * implements LinearAllocator and PoolAllocator for high-performance memory
 * management. these allocators are optimized for specific allocation patterns
 * common in game engines and real-time applications.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#include <luma/core/memory.hpp>
#include <luma/core/logging.hpp>

#include <cstdlib>
#include <new>

namespace luma {

namespace {

/**
 * @brief align pointer to specified alignment
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param[in] ptr pointer to align
 * @param[in] alignment alignment in bytes (must be power of 2)
 * @return aligned pointer
 */
[[nodiscard]] constexpr auto align_pointer(void* ptr, size_t alignment) noexcept -> void* {
    const auto addr = reinterpret_cast<uintptr_t>(ptr);
    const auto aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(aligned_addr);
}

/**
 * @brief calculate padding needed for alignment
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param[in] ptr pointer to check
 * @param[in] alignment alignment in bytes (must be power of 2)
 * @return padding bytes needed
 */
[[nodiscard]] constexpr auto alignment_padding(void* ptr, size_t alignment) noexcept -> size_t {
    const auto addr = reinterpret_cast<uintptr_t>(ptr);
    const auto aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    return aligned_addr - addr;
}

} // anonymous namespace

// =============================================================================
// LinearAllocator Implementation
// =============================================================================

LinearAllocator::LinearAllocator(size_t capacity)
    : capacity_(capacity) {
    
    // allocate backing memory (aligned to max_align_t)
    memory_ = static_cast<u8*>(std::malloc(capacity));
    if (memory_ == nullptr) {
        throw std::bad_alloc();
    }
    
    current_ = memory_;
    
    LOG_TRACE("LinearAllocator created: {} bytes", capacity);
}

LinearAllocator::~LinearAllocator() {
    if (memory_ != nullptr) {
        std::free(memory_);
        LOG_TRACE("LinearAllocator destroyed: {} bytes used / {} bytes capacity",
                  used(), capacity_);
    }
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : memory_(other.memory_)
    , current_(other.current_)
    , capacity_(other.capacity_) {
    
    other.memory_ = nullptr;
    other.current_ = nullptr;
    other.capacity_ = 0;
}

auto LinearAllocator::operator=(LinearAllocator&& other) noexcept -> LinearAllocator& {
    if (this != &other) {
        // free existing memory
        if (memory_ != nullptr) {
            std::free(memory_);
        }
        
        // move from other
        memory_ = other.memory_;
        current_ = other.current_;
        capacity_ = other.capacity_;
        
        // reset other
        other.memory_ = nullptr;
        other.current_ = nullptr;
        other.capacity_ = 0;
    }
    return *this;
}

auto LinearAllocator::create(size_t capacity) -> Result<std::unique_ptr<LinearAllocator>> {
    if (capacity == 0) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "LinearAllocator capacity must be greater than 0"
        });
    }
    
    try {
        auto allocator = std::unique_ptr<LinearAllocator>(new LinearAllocator(capacity));
        return allocator;
    } catch (const std::bad_alloc&) {
        return std::unexpected(Error{
            ErrorCode::CORE_OUT_OF_MEMORY,
            std::format("Failed to allocate {} bytes for LinearAllocator", capacity)
        });
    }
}

auto LinearAllocator::allocate(size_t size, size_t alignment) -> void* {
    if (size == 0) {
        return nullptr;
    }
    
    // calculate aligned address
    void* aligned_ptr = align_pointer(current_, alignment);
    const size_t padding = alignment_padding(current_, alignment);
    
    // check if we have enough space
    const size_t total_size = padding + size;
    if (used() + total_size > capacity_) {
        LOG_ERROR("LinearAllocator out of memory: {} bytes requested, {} bytes available",
                  total_size, remaining());
        return nullptr;
    }
    
    // bump pointer
    current_ = static_cast<u8*>(aligned_ptr) + size;
    
    return aligned_ptr;
}

auto LinearAllocator::reset() noexcept -> void {
    current_ = memory_;
    LOG_TRACE("LinearAllocator reset: {} bytes capacity available", capacity_);
}

auto LinearAllocator::used() const noexcept -> size_t {
    return static_cast<size_t>(current_ - memory_);
}

auto LinearAllocator::capacity() const noexcept -> size_t {
    return capacity_;
}

auto LinearAllocator::remaining() const noexcept -> size_t {
    return capacity_ - used();
}

// =============================================================================
// PoolAllocator Implementation
// =============================================================================

PoolAllocator::PoolAllocator(size_t block_size, size_t block_count)
    : block_size_(block_size >= sizeof(void*) ? block_size : sizeof(void*))
    , block_count_(block_count) {
    
    // allocate backing memory
    const size_t total_size = block_size_ * block_count_;
    memory_ = static_cast<u8*>(std::malloc(total_size));
    if (memory_ == nullptr) {
        throw std::bad_alloc();
    }
    
    // initialize free-list
    initialize_free_list();
    
    LOG_TRACE("PoolAllocator created: {} blocks of {} bytes ({} bytes total)",
              block_count_, block_size_, total_size);
}

PoolAllocator::~PoolAllocator() {
    if (memory_ != nullptr) {
        if (allocated_count_ > 0) {
            LOG_WARN("PoolAllocator destroyed with {} outstanding allocations (memory leak!)",
                     allocated_count_);
        }
        
        std::free(memory_);
        LOG_TRACE("PoolAllocator destroyed: {} blocks allocated / {} blocks total",
                  allocated_count_, block_count_);
    }
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : memory_(other.memory_)
    , free_list_(other.free_list_)
    , block_size_(other.block_size_)
    , block_count_(other.block_count_)
    , allocated_count_(other.allocated_count_) {
    
    other.memory_ = nullptr;
    other.free_list_ = nullptr;
    other.block_size_ = 0;
    other.block_count_ = 0;
    other.allocated_count_ = 0;
}

auto PoolAllocator::operator=(PoolAllocator&& other) noexcept -> PoolAllocator& {
    if (this != &other) {
        // free existing memory
        if (memory_ != nullptr) {
            std::free(memory_);
        }
        
        // move from other
        memory_ = other.memory_;
        free_list_ = other.free_list_;
        block_size_ = other.block_size_;
        block_count_ = other.block_count_;
        allocated_count_ = other.allocated_count_;
        
        // reset other
        other.memory_ = nullptr;
        other.free_list_ = nullptr;
        other.block_size_ = 0;
        other.block_count_ = 0;
        other.allocated_count_ = 0;
    }
    return *this;
}

auto PoolAllocator::create(size_t block_size, size_t block_count)
    -> Result<std::unique_ptr<PoolAllocator>> {
    
    if (block_size == 0) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "PoolAllocator block_size must be greater than 0"
        });
    }
    
    if (block_count == 0) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "PoolAllocator block_count must be greater than 0"
        });
    }
    
    try {
        auto allocator = std::unique_ptr<PoolAllocator>(
            new PoolAllocator(block_size, block_count)
        );
        return allocator;
    } catch (const std::bad_alloc&) {
        return std::unexpected(Error{
            ErrorCode::CORE_OUT_OF_MEMORY,
            std::format("Failed to allocate {} bytes for PoolAllocator",
                       block_size * block_count)
        });
    }
}

auto PoolAllocator::initialize_free_list() -> void {
    // link all blocks in a singly-linked list
    // each block stores a pointer to the next block in its first sizeof(void*) bytes
    
    free_list_ = memory_;
    
    u8* current = memory_;
    for (size_t i = 0; i < block_count_ - 1; ++i) {
        u8* next = current + block_size_;
        *reinterpret_cast<void**>(current) = next;
        current = next;
    }
    
    // last block points to nullptr
    *reinterpret_cast<void**>(current) = nullptr;
}

auto PoolAllocator::allocate() -> void* {
    if (free_list_ == nullptr) {
        LOG_ERROR("PoolAllocator exhausted: all {} blocks in use", block_count_);
        return nullptr;
    }
    
    // pop from free-list
    void* block = free_list_;
    free_list_ = *reinterpret_cast<void**>(free_list_);
    
    ++allocated_count_;
    
    return block;
}

auto PoolAllocator::deallocate(void* ptr) noexcept -> void {
    if (ptr == nullptr) {
        return;
    }
    
    // validate pointer is within pool bounds
    const auto addr = reinterpret_cast<uintptr_t>(ptr);
    const auto start = reinterpret_cast<uintptr_t>(memory_);
    const auto end = start + (block_size_ * block_count_);
    
    if (addr < start || addr >= end) {
        LOG_ERROR("PoolAllocator::deallocate() called with invalid pointer (not from this pool)");
        return;
    }
    
    // validate pointer is block-aligned
    if ((addr - start) % block_size_ != 0) {
        LOG_ERROR("PoolAllocator::deallocate() called with misaligned pointer");
        return;
    }
    
    // push to free-list
    *reinterpret_cast<void**>(ptr) = free_list_;
    free_list_ = ptr;
    
    --allocated_count_;
}

auto PoolAllocator::block_size() const noexcept -> size_t {
    return block_size_;
}

auto PoolAllocator::block_count() const noexcept -> size_t {
    return block_count_;
}

auto PoolAllocator::allocated_count() const noexcept -> size_t {
    return allocated_count_;
}

auto PoolAllocator::free_count() const noexcept -> size_t {
    return block_count_ - allocated_count_;
}

// =============================================================================
// AllocationTracker Implementation
// =============================================================================

std::atomic<size_t> AllocationTracker::total_allocated_{0};
std::atomic<size_t> AllocationTracker::allocation_count_{0};

auto AllocationTracker::record_allocation(size_t size) noexcept -> void {
#ifndef NDEBUG
    total_allocated_.fetch_add(size, std::memory_order_relaxed);
    allocation_count_.fetch_add(1, std::memory_order_relaxed);
#else
    (void)size; // suppress unused parameter warning in release builds
#endif
}

auto AllocationTracker::record_deallocation(size_t size) noexcept -> void {
#ifndef NDEBUG
    total_allocated_.fetch_sub(size, std::memory_order_relaxed);
    allocation_count_.fetch_sub(1, std::memory_order_relaxed);
#else
    (void)size; // suppress unused parameter warning in release builds
#endif
}

auto AllocationTracker::total_allocated() noexcept -> size_t {
    return total_allocated_.load(std::memory_order_relaxed);
}

auto AllocationTracker::allocation_count() noexcept -> size_t {
    return allocation_count_.load(std::memory_order_relaxed);
}

auto AllocationTracker::reset() noexcept -> void {
    total_allocated_.store(0, std::memory_order_relaxed);
    allocation_count_.store(0, std::memory_order_relaxed);
}

} // namespace luma
