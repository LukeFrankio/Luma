/**
 * @file sync.cpp
 * @brief Implementation of Vulkan synchronization primitives
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/sync.hpp>
#include <luma/core/logging.hpp>

namespace luma::vulkan {

// ============================================================================
// Fence Implementation
// ============================================================================

auto Fence::create(VkDevice device, bool signaled) -> Result<Fence> {
    Fence fence;
    
    VkFenceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    if (signaled) {
        create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    
    const auto result = vkCreateFence(device, &create_info, nullptr, &fence.fence_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create fence: {}", static_cast<i32>(result))
        });
    }
    
    fence.device_ = device;
    
    return fence;
}

Fence::~Fence() {
    if (fence_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, fence_, nullptr);
    }
}

Fence::Fence(Fence&& other) noexcept
    : fence_(other.fence_)
    , device_(other.device_) {
    other.fence_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto Fence::operator=(Fence&& other) noexcept -> Fence& {
    if (this != &other) {
        // Cleanup current resources
        this->~Fence();
        
        // Move from other
        fence_ = other.fence_;
        device_ = other.device_;
        
        // Nullify other
        other.fence_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

auto Fence::wait(u64 timeout) const -> Result<void> {
    const auto result = vkWaitForFences(device_, 1, &fence_, VK_TRUE, timeout);
    
    if (result == VK_TIMEOUT) {
        return std::unexpected(Error{
            ErrorCode::TIMEOUT,
            "Fence wait timed out"
        });
    }
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to wait for fence: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto Fence::reset() const -> Result<void> {
    const auto result = vkResetFences(device_, 1, &fence_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to reset fence: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto Fence::is_signaled() const -> bool {
    const auto result = vkGetFenceStatus(device_, fence_);
    return result == VK_SUCCESS;
}

// ============================================================================
// Semaphore Implementation
// ============================================================================

auto Semaphore::create(VkDevice device) -> Result<Semaphore> {
    Semaphore semaphore;
    
    VkSemaphoreCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    const auto result = vkCreateSemaphore(device, &create_info, nullptr, 
                                         &semaphore.semaphore_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create semaphore: {}", static_cast<i32>(result))
        });
    }
    
    semaphore.device_ = device;
    
    return semaphore;
}

Semaphore::~Semaphore() {
    if (semaphore_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, semaphore_, nullptr);
    }
}

Semaphore::Semaphore(Semaphore&& other) noexcept
    : semaphore_(other.semaphore_)
    , device_(other.device_) {
    other.semaphore_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto Semaphore::operator=(Semaphore&& other) noexcept -> Semaphore& {
    if (this != &other) {
        // Cleanup current resources
        this->~Semaphore();
        
        // Move from other
        semaphore_ = other.semaphore_;
        device_ = other.device_;
        
        // Nullify other
        other.semaphore_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

// ============================================================================
// Barrier Helpers Implementation
// ============================================================================

auto create_image_barrier(
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    u32 src_queue_family,
    u32 dst_queue_family,
    VkImageAspectFlags aspect_mask
) -> VkImageMemoryBarrier {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    
    return barrier;
}

auto create_buffer_barrier(
    VkBuffer buffer,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    u32 src_queue_family,
    u32 dst_queue_family,
    VkDeviceSize offset,
    VkDeviceSize size
) -> VkBufferMemoryBarrier {
    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;
    
    return barrier;
}

void insert_pipeline_barrier(
    VkCommandBuffer cmd,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    const std::vector<VkImageMemoryBarrier>& image_barriers,
    const std::vector<VkBufferMemoryBarrier>& buffer_barriers,
    const std::vector<VkMemoryBarrier>& memory_barriers,
    VkDependencyFlags dependency_flags
) {
    vkCmdPipelineBarrier(
        cmd,
        src_stage,
        dst_stage,
        dependency_flags,
        static_cast<u32>(memory_barriers.size()),
        memory_barriers.empty() ? nullptr : memory_barriers.data(),
        static_cast<u32>(buffer_barriers.size()),
        buffer_barriers.empty() ? nullptr : buffer_barriers.data(),
        static_cast<u32>(image_barriers.size()),
        image_barriers.empty() ? nullptr : image_barriers.data()
    );
}

auto transition_image_layout(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkImageAspectFlags aspect_mask
) -> void {
    // Determine access masks and pipeline stages based on layouts
    VkAccessFlags src_access = 0;
    VkAccessFlags dst_access = 0;
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    
    // Source layout
    switch (old_layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            src_access = 0;
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            src_access = VK_ACCESS_TRANSFER_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            src_access = VK_ACCESS_TRANSFER_READ_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            src_access = VK_ACCESS_SHADER_READ_BIT;
            src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            src_access = 0;
            src_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
            
        default:
            LOG_WARN("Unsupported old layout: {}", static_cast<i32>(old_layout));
            break;
    }
    
    // Destination layout
    switch (new_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dst_access = VK_ACCESS_TRANSFER_WRITE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            dst_access = VK_ACCESS_TRANSFER_READ_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            dst_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            dst_access = VK_ACCESS_SHADER_READ_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            dst_access = 0;
            dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            break;
            
        case VK_IMAGE_LAYOUT_GENERAL:
            dst_access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
            
        default:
            LOG_WARN("Unsupported new layout: {}", static_cast<i32>(new_layout));
            break;
    }
    
    // Create barrier
    auto barrier = create_image_barrier(
        image,
        old_layout,
        new_layout,
        src_access,
        dst_access,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        aspect_mask
    );
    
    // Insert barrier
    insert_pipeline_barrier(cmd, src_stage, dst_stage, {barrier}, {}, {});
}

} // namespace luma::vulkan
