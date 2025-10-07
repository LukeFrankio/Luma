/**
 * @file command_buffer.cpp
 * @brief Implementation of Vulkan command buffer management
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/command_buffer.hpp>
#include <luma/core/logging.hpp>

namespace luma::vulkan {

// ============================================================================
// CommandPool Implementation
// ============================================================================

auto CommandPool::create(
    const Device& device,
    u32 queue_family_index,
    VkCommandPoolCreateFlags flags
) -> Result<CommandPool> {
    CommandPool pool;
    
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.queueFamilyIndex = queue_family_index;
    create_info.flags = flags;
    
    const auto result = vkCreateCommandPool(device.handle(), &create_info, 
                                           nullptr, &pool.pool_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create command pool: {}", static_cast<i32>(result))
        });
    }
    
    pool.device_ = device.handle();
    
    LOG_TRACE("Command pool created for queue family {}", queue_family_index);
    
    return pool;
}

CommandPool::~CommandPool() {
    if (pool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, pool_, nullptr);
    }
}

CommandPool::CommandPool(CommandPool&& other) noexcept
    : pool_(other.pool_)
    , device_(other.device_) {
    other.pool_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto CommandPool::operator=(CommandPool&& other) noexcept -> CommandPool& {
    if (this != &other) {
        // Cleanup current resources
        this->~CommandPool();
        
        // Move from other
        pool_ = other.pool_;
        device_ = other.device_;
        
        // Nullify other
        other.pool_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

auto CommandPool::reset(VkCommandPoolResetFlags flags) const -> Result<void> {
    const auto result = vkResetCommandPool(device_, pool_, flags);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to reset command pool: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

// ============================================================================
// CommandBuffer Implementation
// ============================================================================

auto CommandBuffer::allocate(
    const CommandPool& pool,
    VkCommandBufferLevel level
) -> Result<CommandBuffer> {
    CommandBuffer buffer;
    
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool.handle();
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;
    
    const auto result = vkAllocateCommandBuffers(pool.device(), &alloc_info, 
                                                &buffer.buffer_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to allocate command buffer: {}", static_cast<i32>(result))
        });
    }
    
    buffer.pool_ = pool.handle();
    buffer.device_ = pool.device();
    
    return buffer;
}

auto CommandBuffer::allocate_multiple(
    const CommandPool& pool,
    u32 count,
    VkCommandBufferLevel level
) -> Result<std::vector<CommandBuffer>> {
    std::vector<VkCommandBuffer> raw_buffers(count);
    
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool.handle();
    alloc_info.level = level;
    alloc_info.commandBufferCount = count;
    
    const auto result = vkAllocateCommandBuffers(pool.device(), &alloc_info, 
                                                raw_buffers.data());
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to allocate {} command buffers: {}", 
                       count, static_cast<i32>(result))
        });
    }
    
    // Wrap in CommandBuffer objects
    std::vector<CommandBuffer> buffers;
    buffers.reserve(count);
    
    for (auto raw_buffer : raw_buffers) {
        CommandBuffer buffer;
        buffer.buffer_ = raw_buffer;
        buffer.pool_ = pool.handle();
        buffer.device_ = pool.device();
        buffers.push_back(std::move(buffer));
    }
    
    LOG_TRACE("Allocated {} command buffers", count);
    
    return buffers;
}

CommandBuffer::~CommandBuffer() {
    if (buffer_ != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, pool_, 1, &buffer_);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer&& other) noexcept
    : buffer_(other.buffer_)
    , pool_(other.pool_)
    , device_(other.device_) {
    other.buffer_ = VK_NULL_HANDLE;
    other.pool_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto CommandBuffer::operator=(CommandBuffer&& other) noexcept -> CommandBuffer& {
    if (this != &other) {
        // Cleanup current resources
        this->~CommandBuffer();
        
        // Move from other
        buffer_ = other.buffer_;
        pool_ = other.pool_;
        device_ = other.device_;
        
        // Nullify other
        other.buffer_ = VK_NULL_HANDLE;
        other.pool_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

auto CommandBuffer::begin(VkCommandBufferUsageFlags flags) -> Result<void> {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = flags;
    
    const auto result = vkBeginCommandBuffer(buffer_, &begin_info);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to begin command buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto CommandBuffer::end() -> Result<void> {
    const auto result = vkEndCommandBuffer(buffer_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to end command buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto CommandBuffer::reset(VkCommandBufferResetFlags flags) const -> Result<void> {
    const auto result = vkResetCommandBuffer(buffer_, flags);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to reset command buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto CommandBuffer::submit(
    VkQueue queue,
    const std::vector<VkSemaphore>& wait_semaphores,
    const std::vector<VkPipelineStageFlags>& wait_stages,
    const std::vector<VkSemaphore>& signal_semaphores,
    VkFence fence
) const -> Result<void> {
    if (!wait_semaphores.empty() && wait_semaphores.size() != wait_stages.size()) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "wait_semaphores and wait_stages must have same size"
        });
    }
    
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    submit_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.empty() ? nullptr : wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.empty() ? nullptr : wait_stages.data();
    
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buffer_;
    
    submit_info.signalSemaphoreCount = static_cast<u32>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.empty() ? nullptr : signal_semaphores.data();
    
    const auto result = vkQueueSubmit(queue, 1, &submit_info, fence);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to submit command buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto CommandBuffer::submit_and_wait(VkQueue queue) const -> Result<void> {
    // Submit without synchronization
    auto submit_result = submit(queue, {}, {}, {}, VK_NULL_HANDLE);
    if (!submit_result) {
        return submit_result;
    }
    
    // Wait for queue to finish
    const auto result = vkQueueWaitIdle(queue);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to wait for queue: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

} // namespace luma::vulkan
