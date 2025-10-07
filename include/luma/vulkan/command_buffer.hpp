/**
 * @file command_buffer.hpp
 * @brief Vulkan command buffer management for LUMA Engine
 * 
 * This file provides command pool and command buffer wrappers with
 * functional-style recording API. RAII semantics for automatic cleanup uwu
 * 
 * Design decisions:
 * - Command pool per thread (thread-local storage)
 * - Primary and secondary command buffers supported
 * - One-time submit optimization flag
 * - Automatic begin/end tracking
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace luma::vulkan {

/**
 * @class CommandPool
 * @brief Vulkan command pool wrapper with RAII semantics
 * 
 * Manages VkCommandPool for command buffer allocation.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources)
 * 
 * @note Non-copyable, movable
 */
class CommandPool {
public:
    /**
     * @brief Creates command pool
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param device Vulkan device
     * @param queue_family_index Queue family for command pool
     * @param flags Command pool create flags
     * @return Result containing CommandPool or error
     */
    [[nodiscard]] static auto create(
        const Device& device,
        u32 queue_family_index,
        VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    ) -> Result<CommandPool>;
    
    /**
     * @brief Destroys command pool
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~CommandPool();
    
    /**
     * @brief Gets VkCommandPool handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan command pool handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkCommandPool {
        return pool_;
    }
    
    /**
     * @brief Gets VkDevice handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan device handle
     */
    [[nodiscard]] auto device() const noexcept -> VkDevice {
        return device_;
    }
    
    /**
     * @brief Resets command pool
     * 
     * ⚠️ IMPURE FUNCTION (GPU state modification)
     * 
     * @param flags Reset flags
     * @return Result indicating success or error
     */
    auto reset(VkCommandPoolResetFlags flags = 0) const -> Result<void>;
    
    // Non-copyable, movable
    CommandPool(const CommandPool&) = delete;
    auto operator=(const CommandPool&) -> CommandPool& = delete;
    
    CommandPool(CommandPool&& other) noexcept;
    auto operator=(CommandPool&& other) noexcept -> CommandPool&;
    
private:
    CommandPool() = default;
    
    VkCommandPool pool_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
};

/**
 * @class CommandBuffer
 * @brief Vulkan command buffer wrapper with RAII semantics
 * 
 * Manages VkCommandBuffer with functional recording API.
 * 
 * ⚠️ IMPURE CLASS (records GPU commands)
 * 
 * @note Non-copyable, movable
 */
class CommandBuffer {
public:
    /**
     * @brief Allocates command buffer from pool
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param pool Command pool to allocate from
     * @param level Command buffer level (primary or secondary)
     * @return Result containing CommandBuffer or error
     */
    [[nodiscard]] static auto allocate(
        const CommandPool& pool,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    ) -> Result<CommandBuffer>;
    
    /**
     * @brief Allocates multiple command buffers
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param pool Command pool to allocate from
     * @param count Number of command buffers
     * @param level Command buffer level
     * @return Result containing vector of CommandBuffers or error
     */
    [[nodiscard]] static auto allocate_multiple(
        const CommandPool& pool,
        u32 count,
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY
    ) -> Result<std::vector<CommandBuffer>>;
    
    /**
     * @brief Frees command buffer
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~CommandBuffer();
    
    /**
     * @brief Gets VkCommandBuffer handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan command buffer handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkCommandBuffer {
        return buffer_;
    }
    
    /**
     * @brief Begins command buffer recording
     * 
     * ⚠️ IMPURE FUNCTION (GPU state modification)
     * 
     * @param flags Begin flags (one-time submit, etc.)
     * @return Result indicating success or error
     */
    auto begin(VkCommandBufferUsageFlags flags = 0) -> Result<void>;
    
    /**
     * @brief Ends command buffer recording
     * 
     * ⚠️ IMPURE FUNCTION (GPU state modification)
     * 
     * @return Result indicating success or error
     */
    auto end() -> Result<void>;
    
    /**
     * @brief Resets command buffer
     * 
     * ⚠️ IMPURE FUNCTION (GPU state modification)
     * 
     * @param flags Reset flags
     * @return Result indicating success or error
     */
    auto reset(VkCommandBufferResetFlags flags = 0) const -> Result<void>;
    
    /**
     * @brief Submits command buffer to queue
     * 
     * ⚠️ IMPURE FUNCTION (GPU command submission)
     * 
     * @param queue Queue to submit to
     * @param wait_semaphores Semaphores to wait on
     * @param wait_stages Pipeline stages to wait at
     * @param signal_semaphores Semaphores to signal
     * @param fence Fence to signal on completion
     * @return Result indicating success or error
     */
    auto submit(
        VkQueue queue,
        const std::vector<VkSemaphore>& wait_semaphores = {},
        const std::vector<VkPipelineStageFlags>& wait_stages = {},
        const std::vector<VkSemaphore>& signal_semaphores = {},
        VkFence fence = VK_NULL_HANDLE
    ) const -> Result<void>;
    
    /**
     * @brief Submits command buffer and waits for completion
     * 
     * ⚠️ IMPURE FUNCTION (GPU command submission and sync)
     * 
     * @param queue Queue to submit to
     * @return Result indicating success or error
     */
    auto submit_and_wait(VkQueue queue) const -> Result<void>;
    
    // Non-copyable, movable
    CommandBuffer(const CommandBuffer&) = delete;
    auto operator=(const CommandBuffer&) -> CommandBuffer& = delete;
    
    CommandBuffer(CommandBuffer&& other) noexcept;
    auto operator=(CommandBuffer&& other) noexcept -> CommandBuffer&;
    
private:
    CommandBuffer() = default;
    
    VkCommandBuffer buffer_ = VK_NULL_HANDLE;
    VkCommandPool pool_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    bool is_recording_ = false;
};

} // namespace luma::vulkan
