/**
 * @file memory.hpp
 * @brief custom memory allocators for high-performance memory management uwu
 * 
 * this file implements custom allocators optimized for specific use cases:
 * - LinearAllocator: fast bump-pointer allocation for temporary data
 * - PoolAllocator: fixed-size block allocation with free-list
 * 
 * these allocators are designed for the LUMA engine's memory management
 * patterns, where we have predictable allocation patterns and can benefit
 * from custom allocators over general-purpose malloc/new.
 * 
 * design philosophy:
 * - fast allocation: minimize overhead, maximize cache locality
 * - predictable performance: O(1) allocation and deallocation
 * - no fragmentation: linear allocator never fragments, pool allocator uses fixed sizes
 * - debug tracking: optional allocation tracking for leak detection
 * 
 * use cases:
 * - LinearAllocator: frame-temp data (command buffers, intermediate results)
 * - PoolAllocator: entities, components, jobs (fixed-size objects)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note uses C++26 features (latest standard, even beta features ok!)
 * @note compiled with GCC 15+ (latest version preferred)
 * @warning thread safety varies by allocator (see individual docs)
 * 
 * example usage (LinearAllocator):
 * @code
 * auto allocator = LinearAllocator::create(1024 * 1024);  // 1 MB
 * 
 * // allocate temporary data
 * auto* buffer = allocator->allocate<float>(256);
 * // use buffer...
 * 
 * // reset entire allocator (free all at once)
 * allocator->reset();
 * @endcode
 * 
 * example usage (PoolAllocator):
 * @code
 * auto allocator = PoolAllocator::create(sizeof(Entity), 1000);
 * 
 * // allocate entity
 * auto* entity = allocator->allocate<Entity>();
 * new (entity) Entity{};  // placement new
 * 
 * // deallocate entity
 * entity->~Entity();
 * allocator->deallocate(entity);
 * @endcode
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/core/math.hpp>

#include <memory>
#include <cstddef>
#include <cstring>
#include <atomic>

namespace luma {

/**
 * @brief linear allocator (bump pointer, fast allocation, no individual free)
 * 
 * linear allocator uses a simple bump-pointer algorithm: maintain a pointer
 * to the next free address, increment by allocation size. extremely fast
 * allocation (just pointer increment), but no individual deallocation. instead,
 * reset entire allocator at once (e.g., once per frame).
 * 
 * characteristics:
 * - allocation: O(1), just pointer increment
 * - deallocation: not supported (must reset entire allocator)
 * - reset: O(1), just reset pointer to start
 * - fragmentation: zero (contiguous allocations)
 * - thread safety: not thread-safe (use per-thread allocators)
 * 
 * use cases:
 * - frame-temporary data (command buffers, intermediate results)
 * - any data with stack-like lifetime (allocate, use, free all at once)
 * 
 * @note not thread-safe (create one per thread if needed)
 * @note no individual deallocation (must reset entire allocator)
 * @note fast allocation makes it ideal for frame-temp data
 * 
 * @see PoolAllocator for fixed-size allocations with individual free
 */
class LinearAllocator {
public:
    /**
     * @brief create linear allocator with specified capacity
     * 
     * ⚠️ IMPURE FUNCTION (allocates memory)
     * 
     * allocates a contiguous block of memory of the specified size. this
     * memory is reused for all allocations until reset.
     * 
     * @param[in] capacity total size in bytes
     * 
     * @return Result containing LinearAllocator unique_ptr or error
     * @retval OK if successful
     * @retval OUT_OF_MEMORY if allocation failed
     * 
     * @pre capacity > 0
     * @post allocator is ready for allocations
     * 
     * @note prefer large capacity (e.g., 1-4 MB per frame)
     * @note allocations fail if capacity exceeded
     * 
     * @complexity O(1) (single large allocation)
     * 
     * example:
     * @code
     * auto allocator = LinearAllocator::create(1024 * 1024);  // 1 MB
     * if (!allocator) {
     *     LOG_ERROR("Failed to create allocator: {}", allocator.error().message());
     *     return;
     * }
     * @endcode
     */
    [[nodiscard]] static auto create(size_t capacity) -> Result<std::unique_ptr<LinearAllocator>>;
    
    /**
     * @brief destructor (frees backing memory)
     * 
     * ⚠️ IMPURE FUNCTION (frees memory)
     * 
     * frees the backing memory block. any pointers returned by allocate()
     * become invalid.
     * 
     * @warning dangling pointers (ensure no allocations are in use)
     */
    ~LinearAllocator();
    
    // non-copyable, movable
    LinearAllocator(const LinearAllocator&) = delete;
    auto operator=(const LinearAllocator&) -> LinearAllocator& = delete;
    LinearAllocator(LinearAllocator&&) noexcept;
    auto operator=(LinearAllocator&&) noexcept -> LinearAllocator&;
    
    /**
     * @brief allocate memory with alignment
     * 
     * ⚠️ IMPURE FUNCTION (modifies allocator state)
     * 
     * allocates memory of the specified size with the specified alignment.
     * returns nullptr if capacity exceeded.
     * 
     * @param[in] size size in bytes
     * @param[in] alignment alignment in bytes (must be power of 2)
     * 
     * @return pointer to allocated memory or nullptr if failed
     * 
     * @pre size > 0
     * @pre alignment is power of 2
     * @post pointer is aligned to alignment boundary
     * 
     * @note allocation never fails due to fragmentation (contiguous)
     * @note allocation fails if capacity exceeded
     * @note pointer remains valid until reset() or destructor
     * 
     * @complexity O(1) (pointer increment + alignment)
     * 
     * example:
     * @code
     * void* ptr = allocator->allocate(256, 16);  // 256 bytes, 16-byte aligned
     * if (ptr == nullptr) {
     *     LOG_ERROR("Allocator out of memory!");
     * }
     * @endcode
     */
    [[nodiscard]] auto allocate(size_t size, size_t alignment = alignof(std::max_align_t)) -> void*;
    
    /**
     * @brief allocate typed memory (template convenience)
     * 
     * ⚠️ IMPURE FUNCTION (modifies allocator state)
     * 
     * allocates memory for N objects of type T with proper alignment.
     * returns nullptr if capacity exceeded.
     * 
     * @tparam T type of objects to allocate
     * @param[in] count number of objects
     * 
     * @return pointer to allocated memory or nullptr if failed
     * 
     * @pre count > 0
     * @post pointer is aligned to alignof(T)
     * 
     * @note does NOT call constructors (use placement new if needed)
     * @note pointer remains valid until reset() or destructor
     * 
     * @complexity O(1)
     * 
     * example:
     * @code
     * float* buffer = allocator->allocate<float>(256);
     * if (buffer != nullptr) {
     *     // use buffer (no constructor called)
     * }
     * @endcode
     */
    template<typename T>
    [[nodiscard]] auto allocate(size_t count = 1) -> T* {
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }
    
    /**
     * @brief reset allocator (free all allocations)
     * 
     * ⚠️ IMPURE FUNCTION (modifies allocator state)
     * 
     * resets the allocator to its initial state, effectively freeing all
     * allocations at once. this is extremely fast (just pointer reset).
     * 
     * @post all previous allocations are invalid (dangling pointers)
     * @post allocator is ready for new allocations
     * 
     * @warning all pointers returned by allocate() become invalid
     * @note typically called once per frame (frame-temp data pattern)
     * 
     * @complexity O(1) (pointer reset)
     * 
     * example:
     * @code
     * // frame loop
     * while (running) {
     *     allocator->reset();  // free all frame-temp data
     *     
     *     // allocate new frame-temp data
     *     auto* buffer = allocator->allocate<float>(256);
     *     // use buffer...
     * }
     * @endcode
     */
    auto reset() noexcept -> void;
    
    /**
     * @brief get current memory usage
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return number of bytes currently allocated
     */
    [[nodiscard]] auto used() const noexcept -> size_t;
    
    /**
     * @brief get total capacity
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return total capacity in bytes
     */
    [[nodiscard]] auto capacity() const noexcept -> size_t;
    
    /**
     * @brief get remaining capacity
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return remaining capacity in bytes
     */
    [[nodiscard]] auto remaining() const noexcept -> size_t;
    
private:
    /**
     * @brief private constructor (use create() factory function)
     * 
     * @param[in] capacity total size in bytes
     */
    explicit LinearAllocator(size_t capacity);
    
    u8* memory_{nullptr};      ///< backing memory block
    u8* current_{nullptr};     ///< current allocation pointer
    size_t capacity_{0};       ///< total capacity
};

/**
 * @brief pool allocator (fixed-size blocks, free-list)
 * 
 * pool allocator manages a pool of fixed-size blocks. allocations return
 * a block from the free-list, deallocations return the block to the free-list.
 * extremely fast allocation/deallocation (O(1) linked-list operations).
 * 
 * characteristics:
 * - allocation: O(1), pop from free-list
 * - deallocation: O(1), push to free-list
 * - fragmentation: zero (all blocks same size)
 * - thread safety: not thread-safe (use per-thread pools or add locking)
 * 
 * use cases:
 * - entities (fixed-size Entity struct)
 * - components (fixed-size Component struct)
 * - jobs (fixed-size Job struct)
 * 
 * @note not thread-safe (create one per thread or add locking)
 * @note all blocks must be same size (enforce at runtime)
 * @note deallocated blocks can be reused (no fragmentation)
 * 
 * @see LinearAllocator for temporary allocations without individual free
 */
class PoolAllocator {
public:
    /**
     * @brief create pool allocator with specified block size and count
     * 
     * ⚠️ IMPURE FUNCTION (allocates memory)
     * 
     * allocates a contiguous block of memory containing the specified number
     * of fixed-size blocks. all blocks are initially added to the free-list.
     * 
     * @param[in] block_size size of each block in bytes
     * @param[in] block_count number of blocks in pool
     * 
     * @return Result containing PoolAllocator unique_ptr or error
     * @retval OK if successful
     * @retval OUT_OF_MEMORY if allocation failed
     * 
     * @pre block_size >= sizeof(void*) (for free-list pointer)
     * @pre block_count > 0
     * @post allocator is ready for allocations
     * 
     * @note prefer large block_count (e.g., 1000-10000)
     * @note allocations fail if pool exhausted (all blocks in use)
     * 
     * @complexity O(n) where n = block_count (initialize free-list)
     * 
     * example:
     * @code
     * auto allocator = PoolAllocator::create(sizeof(Entity), 1000);
     * if (!allocator) {
     *     LOG_ERROR("Failed to create allocator: {}", allocator.error().message());
     *     return;
     * }
     * @endcode
     */
    [[nodiscard]] static auto create(size_t block_size, size_t block_count)
        -> Result<std::unique_ptr<PoolAllocator>>;
    
    /**
     * @brief destructor (frees backing memory)
     * 
     * ⚠️ IMPURE FUNCTION (frees memory)
     * 
     * frees the backing memory block. any pointers returned by allocate()
     * become invalid.
     * 
     * @warning dangling pointers (ensure all blocks deallocated)
     * @warning does NOT call destructors (user's responsibility)
     */
    ~PoolAllocator();
    
    // non-copyable, movable
    PoolAllocator(const PoolAllocator&) = delete;
    auto operator=(const PoolAllocator&) -> PoolAllocator& = delete;
    PoolAllocator(PoolAllocator&&) noexcept;
    auto operator=(PoolAllocator&&) noexcept -> PoolAllocator&;
    
    /**
     * @brief allocate a block
     * 
     * ⚠️ IMPURE FUNCTION (modifies free-list)
     * 
     * allocates a block from the free-list. returns nullptr if pool exhausted.
     * 
     * @return pointer to allocated block or nullptr if failed
     * 
     * @post pointer is aligned to block_size boundary
     * 
     * @note does NOT call constructors (use placement new if needed)
     * @note pointer remains valid until deallocate() or destructor
     * @note allocation fails if all blocks in use
     * 
     * @complexity O(1) (pop from free-list)
     * 
     * example:
     * @code
     * void* ptr = allocator->allocate();
     * if (ptr == nullptr) {
     *     LOG_ERROR("Pool exhausted!");
     * }
     * @endcode
     */
    [[nodiscard]] auto allocate() -> void*;
    
    /**
     * @brief allocate typed block (template convenience)
     * 
     * ⚠️ IMPURE FUNCTION (modifies free-list)
     * 
     * allocates a block suitable for type T. returns nullptr if pool exhausted.
     * 
     * @tparam T type of object to allocate
     * 
     * @return pointer to allocated block or nullptr if failed
     * 
     * @pre sizeof(T) <= block_size
     * @post pointer is aligned to alignof(T)
     * 
     * @note does NOT call constructor (use placement new if needed)
     * @note pointer remains valid until deallocate() or destructor
     * 
     * @complexity O(1)
     * 
     * example:
     * @code
     * auto* entity = allocator->allocate<Entity>();
     * if (entity != nullptr) {
     *     new (entity) Entity{};  // placement new (call constructor)
     * }
     * @endcode
     */
    template<typename T>
    [[nodiscard]] auto allocate() -> T* {
        static_assert(sizeof(T) <= sizeof(void*) * 16, "Type too large for typical pool block");
        return static_cast<T*>(allocate());
    }
    
    /**
     * @brief deallocate a block
     * 
     * ⚠️ IMPURE FUNCTION (modifies free-list)
     * 
     * returns a block to the free-list, making it available for reuse.
     * 
     * @param[in] ptr pointer to block (must be from this allocator)
     * 
     * @pre ptr was allocated by this allocator
     * @pre ptr has not been deallocated already (double-free is UB)
     * @post ptr is invalid (dangling pointer)
     * @post block is available for reuse
     * 
     * @warning does NOT call destructor (user's responsibility)
     * @warning double-free is undefined behavior
     * @warning ptr becomes invalid (dangling pointer)
     * 
     * @complexity O(1) (push to free-list)
     * 
     * example:
     * @code
     * auto* entity = allocator->allocate<Entity>();
     * new (entity) Entity{};
     * 
     * // later...
     * entity->~Entity();  // call destructor manually
     * allocator->deallocate(entity);
     * @endcode
     */
    auto deallocate(void* ptr) noexcept -> void;
    
    /**
     * @brief get block size
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return size of each block in bytes
     */
    [[nodiscard]] auto block_size() const noexcept -> size_t;
    
    /**
     * @brief get total block count
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return total number of blocks in pool
     */
    [[nodiscard]] auto block_count() const noexcept -> size_t;
    
    /**
     * @brief get number of allocated blocks
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return number of blocks currently in use
     */
    [[nodiscard]] auto allocated_count() const noexcept -> size_t;
    
    /**
     * @brief get number of free blocks
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return number of blocks available for allocation
     */
    [[nodiscard]] auto free_count() const noexcept -> size_t;
    
private:
    /**
     * @brief private constructor (use create() factory function)
     * 
     * @param[in] block_size size of each block
     * @param[in] block_count number of blocks
     */
    PoolAllocator(size_t block_size, size_t block_count);
    
    /**
     * @brief initialize free-list
     * 
     * links all blocks together in a singly-linked list
     */
    auto initialize_free_list() -> void;
    
    u8* memory_{nullptr};           ///< backing memory block
    void* free_list_{nullptr};      ///< head of free-list (linked list)
    size_t block_size_{0};          ///< size of each block
    size_t block_count_{0};         ///< total number of blocks
    size_t allocated_count_{0};     ///< number of allocated blocks
};

/**
 * @brief allocation tracker (debug helper)
 * 
 * tracks allocations and deallocations for leak detection. this is a simple
 * counter-based tracker (not a full memory profiler).
 * 
 * @note only enabled in debug builds (zero overhead in release)
 * @note thread-safe (uses atomic counters)
 */
class AllocationTracker {
public:
    /**
     * @brief record allocation
     * 
     * @param[in] size size in bytes
     */
    static auto record_allocation(size_t size) noexcept -> void;
    
    /**
     * @brief record deallocation
     * 
     * @param[in] size size in bytes
     */
    static auto record_deallocation(size_t size) noexcept -> void;
    
    /**
     * @brief get total allocated bytes
     * 
     * @return total bytes currently allocated
     */
    [[nodiscard]] static auto total_allocated() noexcept -> size_t;
    
    /**
     * @brief get allocation count
     * 
     * @return number of outstanding allocations
     */
    [[nodiscard]] static auto allocation_count() noexcept -> size_t;
    
    /**
     * @brief reset counters
     */
    static auto reset() noexcept -> void;
    
private:
    static std::atomic<size_t> total_allocated_;
    static std::atomic<size_t> allocation_count_;
};

} // namespace luma
