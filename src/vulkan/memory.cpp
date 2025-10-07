/**
 * @file memory.cpp
 * @brief Implementation of VMA-based memory management
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/memory.hpp>
#include <luma/core/logging.hpp>

#include <cstring>

namespace luma::vulkan {

// ============================================================================
// Allocator Implementation
// ============================================================================

auto Allocator::create(
    const Instance& instance,
    const Device& device,
    VmaAllocatorCreateFlags flags
) -> Result<Allocator> {
    Allocator allocator;
    
    LOG_INFO("Creating VMA allocator...");
    
    VmaAllocatorCreateInfo create_info = {};
    create_info.flags = flags;
    create_info.physicalDevice = device.physical_device();
    create_info.device = device.handle();
    create_info.instance = instance.handle();
    
    // Use Vulkan 1.3
    create_info.vulkanApiVersion = VK_API_VERSION_1_3;
    
    const auto result = vmaCreateAllocator(&create_info, &allocator.allocator_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create VMA allocator: {}", static_cast<i32>(result))
        });
    }
    
    allocator.device_ = device.handle();
    
    LOG_INFO("VMA allocator created successfully");
    
    return allocator;
}

Allocator::~Allocator() {
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        LOG_INFO("VMA allocator destroyed");
    }
}

Allocator::Allocator(Allocator&& other) noexcept
    : allocator_(other.allocator_)
    , device_(other.device_) {
    other.allocator_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto Allocator::operator=(Allocator&& other) noexcept -> Allocator& {
    if (this != &other) {
        // Cleanup current resources
        this->~Allocator();
        
        // Move from other
        allocator_ = other.allocator_;
        device_ = other.device_;
        
        // Nullify other
        other.allocator_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

// ============================================================================
// Buffer Implementation
// ============================================================================

auto Buffer::create(
    const Allocator& allocator,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memory_usage,
    VmaAllocationCreateFlags flags
) -> Result<Buffer> {
    if (size == 0) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "Buffer size cannot be zero"
        });
    }
    
    Buffer buffer;
    
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = memory_usage;
    alloc_info.flags = flags;
    
    const auto result = vmaCreateBuffer(
        allocator.handle(),
        &buffer_info,
        &alloc_info,
        &buffer.buffer_,
        &buffer.allocation_,
        nullptr
    );
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to create buffer: {}", static_cast<i32>(result))
        });
    }
    
    buffer.allocator_ = allocator.handle();
    buffer.size_ = size;
    
    LOG_TRACE("Created buffer (size: {} bytes)", size);
    
    return buffer;
}

Buffer::~Buffer() {
    if (buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
        if (mapped_data_ != nullptr) {
            vmaUnmapMemory(allocator_, allocation_);
        }
        
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : buffer_(other.buffer_)
    , allocation_(other.allocation_)
    , allocator_(other.allocator_)
    , size_(other.size_)
    , mapped_data_(other.mapped_data_) {
    other.buffer_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.allocator_ = VK_NULL_HANDLE;
    other.size_ = 0;
    other.mapped_data_ = nullptr;
}

auto Buffer::operator=(Buffer&& other) noexcept -> Buffer& {
    if (this != &other) {
        // Cleanup current resources
        this->~Buffer();
        
        // Move from other
        buffer_ = other.buffer_;
        allocation_ = other.allocation_;
        allocator_ = other.allocator_;
        size_ = other.size_;
        mapped_data_ = other.mapped_data_;
        
        // Nullify other
        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.allocator_ = VK_NULL_HANDLE;
        other.size_ = 0;
        other.mapped_data_ = nullptr;
    }
    
    return *this;
}

auto Buffer::map() -> Result<void*> {
    if (mapped_data_ != nullptr) {
        return mapped_data_;
    }
    
    const auto result = vmaMapMemory(allocator_, allocation_, &mapped_data_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to map buffer memory: {}", static_cast<i32>(result))
        });
    }
    
    return mapped_data_;
}

void Buffer::unmap() {
    if (mapped_data_ != nullptr) {
        vmaUnmapMemory(allocator_, allocation_);
        mapped_data_ = nullptr;
    }
}

auto Buffer::flush(VkDeviceSize offset, VkDeviceSize size) const -> Result<void> {
    const auto flush_size = (size == VK_WHOLE_SIZE) ? size_ : size;
    
    const auto result = vmaFlushAllocation(allocator_, allocation_, offset, flush_size);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to flush buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto Buffer::invalidate(VkDeviceSize offset, VkDeviceSize size) const -> Result<void> {
    const auto invalidate_size = (size == VK_WHOLE_SIZE) ? size_ : size;
    
    const auto result = vmaInvalidateAllocation(allocator_, allocation_, offset, invalidate_size);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to invalidate buffer: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

// ============================================================================
// Image Implementation
// ============================================================================

auto Image::create(
    const Allocator& allocator,
    u32 width,
    u32 height,
    VkFormat format,
    VkImageUsageFlags usage,
    VmaMemoryUsage memory_usage,
    VmaAllocationCreateFlags flags
) -> Result<Image> {
    if (width == 0 || height == 0) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "Image dimensions cannot be zero"
        });
    }
    
    Image image;
    
    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = memory_usage;
    alloc_info.flags = flags;
    
    const auto result = vmaCreateImage(
        allocator.handle(),
        &image_info,
        &alloc_info,
        &image.image_,
        &image.allocation_,
        nullptr
    );
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to create image: {}", static_cast<i32>(result))
        });
    }
    
    image.allocator_ = allocator.handle();
    image.device_ = allocator.device();
    image.width_ = width;
    image.height_ = height;
    image.format_ = format;
    
    // Create image view
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image.image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    
    const auto view_result = vkCreateImageView(allocator.device(), &view_info, 
                                              nullptr, &image.view_);
    
    if (view_result != VK_SUCCESS) {
        vmaDestroyImage(allocator.handle(), image.image_, image.allocation_);
        
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to create image view: {}", static_cast<i32>(view_result))
        });
    }
    
    LOG_TRACE("Created image ({}x{}, format: {})", width, height, static_cast<i32>(format));
    
    return image;
}

Image::~Image() {
    if (device_ != VK_NULL_HANDLE) {
        if (view_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, view_, nullptr);
        }
        
        if (image_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator_, image_, allocation_);
        }
    }
}

Image::Image(Image&& other) noexcept
    : image_(other.image_)
    , view_(other.view_)
    , allocation_(other.allocation_)
    , allocator_(other.allocator_)
    , device_(other.device_)
    , width_(other.width_)
    , height_(other.height_)
    , format_(other.format_) {
    other.image_ = VK_NULL_HANDLE;
    other.view_ = VK_NULL_HANDLE;
    other.allocation_ = VK_NULL_HANDLE;
    other.allocator_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
    other.width_ = 0;
    other.height_ = 0;
}

auto Image::operator=(Image&& other) noexcept -> Image& {
    if (this != &other) {
        // Cleanup current resources
        this->~Image();
        
        // Move from other
        image_ = other.image_;
        view_ = other.view_;
        allocation_ = other.allocation_;
        allocator_ = other.allocator_;
        device_ = other.device_;
        width_ = other.width_;
        height_ = other.height_;
        format_ = other.format_;
        
        // Nullify other
        other.image_ = VK_NULL_HANDLE;
        other.view_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.allocator_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.width_ = 0;
        other.height_ = 0;
    }
    
    return *this;
}

// ============================================================================
// Helper Functions Implementation
// ============================================================================

auto copy_buffer(
    VkCommandBuffer cmd,
    VkBuffer src,
    VkBuffer dst,
    VkDeviceSize size,
    VkDeviceSize src_offset,
    VkDeviceSize dst_offset
) -> void {
    VkBufferCopy copy_region = {};
    copy_region.srcOffset = src_offset;
    copy_region.dstOffset = dst_offset;
    copy_region.size = size;
    
    vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);
}

auto copy_buffer_to_image(
    VkCommandBuffer cmd,
    VkBuffer buffer,
    VkImage image,
    u32 width,
    u32 height,
    VkDeviceSize buffer_offset
) -> void {
    VkBufferImageCopy region = {};
    region.bufferOffset = buffer_offset;
    region.bufferRowLength = 0;  // Tightly packed
    region.bufferImageHeight = 0;  // Tightly packed
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(
        cmd,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
}

auto copy_image_to_buffer(
    VkCommandBuffer cmd,
    VkImage image,
    VkBuffer buffer,
    u32 width,
    u32 height,
    VkDeviceSize buffer_offset
) -> void {
    VkBufferImageCopy region = {};
    region.bufferOffset = buffer_offset;
    region.bufferRowLength = 0;  // Tightly packed
    region.bufferImageHeight = 0;  // Tightly packed
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyImageToBuffer(
        cmd,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buffer,
        1,
        &region
    );
}

} // namespace luma::vulkan
