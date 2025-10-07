/**
 * @file sync.hpp
 * @brief Vulkan synchronization primitives for LUMA Engine
 * 
 * This file provides RAII wrappers for fences, semaphores, and barrier
 * helpers. Manages GPU-GPU and CPU-GPU synchronization uwu
 * 
 * Design decisions:
 * - RAII wrappers for automatic cleanup
 * - Timeline semaphores for advanced sync (Vulkan 1.2+)
 * - Fence pooling for reuse
 * - Pipeline barrier helpers for common patterns
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>

#include <vulkan/vulkan.h>

#include <chrono>

namespace luma::vulkan {

/**
 * @class Fence
 * @brief Vulkan fence wrapper with RAII semantics
 * 
 * Manages VkFence for CPU-GPU synchronization.
 * 
 * ⚠️ IMPURE CLASS (manages GPU sync primitives)
 * 
 * @note Non-copyable, movable
 */
class Fence {
public:
    /**
     * @brief Creates fence
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param device Vulkan device handle
     * @param signaled Create fence in signaled state
     * @return Result containing Fence or error
     */
    [[nodiscard]] static auto create(
        VkDevice device,
        bool signaled = false
    ) -> Result<Fence>;
    
    /**
     * @brief Destroys fence
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Fence();
    
    /**
     * @brief Gets VkFence handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan fence handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkFence {
        return fence_;
    }
    
    /**
     * @brief Waits for fence to be signaled
     * 
     * ⚠️ IMPURE FUNCTION (GPU synchronization)
     * 
     * @param timeout Timeout in nanoseconds (default: UINT64_MAX)
     * @return Result indicating success or timeout
     */
    auto wait(u64 timeout = UINT64_MAX) const -> Result<void>;
    
    /**
     * @brief Resets fence to unsignaled state
     * 
     * ⚠️ IMPURE FUNCTION (GPU state modification)
     * 
     * @return Result indicating success or error
     */
    auto reset() const -> Result<void>;
    
    /**
     * @brief Checks if fence is signaled
     * 
     * ⚠️ IMPURE FUNCTION (queries GPU state)
     * 
     * @return true if fence is signaled
     */
    [[nodiscard]] auto is_signaled() const -> bool;
    
    // Non-copyable, movable
    Fence(const Fence&) = delete;
    auto operator=(const Fence&) -> Fence& = delete;
    
    Fence(Fence&& other) noexcept;
    auto operator=(Fence&& other) noexcept -> Fence&;
    
private:
    Fence() = default;
    
    VkFence fence_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
};

/**
 * @class Semaphore
 * @brief Vulkan semaphore wrapper with RAII semantics
 * 
 * Manages VkSemaphore for GPU-GPU synchronization.
 * 
 * ⚠️ IMPURE CLASS (manages GPU sync primitives)
 * 
 * @note Non-copyable, movable
 */
class Semaphore {
public:
    /**
     * @brief Creates semaphore
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param device Vulkan device handle
     * @return Result containing Semaphore or error
     */
    [[nodiscard]] static auto create(
        VkDevice device
    ) -> Result<Semaphore>;
    
    /**
     * @brief Destroys semaphore
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Semaphore();
    
    /**
     * @brief Gets VkSemaphore handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan semaphore handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkSemaphore {
        return semaphore_;
    }
    
    // Non-copyable, movable
    Semaphore(const Semaphore&) = delete;
    auto operator=(const Semaphore&) -> Semaphore& = delete;
    
    Semaphore(Semaphore&& other) noexcept;
    auto operator=(Semaphore&& other) noexcept -> Semaphore&;
    
private:
    Semaphore() = default;
    
    VkSemaphore semaphore_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
};

/**
 * @brief Creates image memory barrier
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param image Image to barrier
 * @param old_layout Old image layout
 * @param new_layout New image layout
 * @param src_access_mask Source access mask
 * @param dst_access_mask Destination access mask
 * @param aspect_mask Image aspect mask
 * @return Image memory barrier
 */
[[nodiscard]] auto create_image_barrier(
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags src_access_mask,
    VkAccessFlags dst_access_mask,
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
) -> VkImageMemoryBarrier;

/**
 * @brief Creates buffer memory barrier
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param buffer Buffer to barrier
 * @param src_access_mask Source access mask
 * @param dst_access_mask Destination access mask
 * @param size Buffer size (VK_WHOLE_SIZE for entire buffer)
 * @param offset Buffer offset
 * @return Buffer memory barrier
 */
[[nodiscard]] auto create_buffer_barrier(
    VkBuffer buffer,
    VkAccessFlags src_access_mask,
    VkAccessFlags dst_access_mask,
    VkDeviceSize size = VK_WHOLE_SIZE,
    VkDeviceSize offset = 0
) -> VkBufferMemoryBarrier;

/**
 * @brief Inserts pipeline barrier into command buffer
 * 
 * ⚠️ IMPURE FUNCTION (records GPU command)
 * 
 * @param cmd_buffer Command buffer
 * @param src_stage Source pipeline stage
 * @param dst_stage Destination pipeline stage
 * @param image_barriers Image barriers
 * @param buffer_barriers Buffer barriers
 * @param memory_barriers Memory barriers
 * @param dependency_flags Dependency flags
 */
auto insert_pipeline_barrier(
    VkCommandBuffer cmd_buffer,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    const std::vector<VkImageMemoryBarrier>& image_barriers = {},
    const std::vector<VkBufferMemoryBarrier>& buffer_barriers = {},
    const std::vector<VkMemoryBarrier>& memory_barriers = {},
    VkDependencyFlags dependency_flags = 0
) -> void;

} // namespace luma::vulkan
