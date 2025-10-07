/**
 * @file memory.hpp
 * @brief Vulkan memory management with VMA for LUMA Engine
 * 
 * This file provides buffer and image wrappers using Vulkan Memory Allocator
 * (VMA) for efficient GPU memory management. Handles unified memory on iGPUs uwu
 * 
 * Design decisions:
 * - Use VMA for all allocations (zero manual memory management)
 * - Prefer DEVICE_LOCAL for GPU-only resources
 * - Use HOST_VISIBLE | DEVICE_LOCAL on iGPUs (unified memory)
 * - RAII wrappers for automatic cleanup
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>
#include <luma/vulkan/instance.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <span>
#include <cstring>

namespace luma::vulkan {

/**
 * @class Allocator
 * @brief VMA allocator wrapper with RAII semantics
 * 
 * Manages VmaAllocator instance. Should be created once per device.
 * 
 * ⚠️ IMPURE CLASS (manages GPU memory)
 * 
 * @note Non-copyable, movable
 */
class Allocator {
public:
    /**
     * @brief Creates VMA allocator
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param instance Vulkan instance
     * @param device Vulkan device
     * @param flags Optional allocator creation flags
     * @return Result containing Allocator or error
     */
    [[nodiscard]] static auto create(
        const Instance& instance,
        const Device& device,
        VmaAllocatorCreateFlags flags = 0
    ) -> Result<Allocator>;
    
    /**
     * @brief Destroys VMA allocator
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Allocator();
    
    /**
     * @brief Gets VmaAllocator handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return VMA allocator handle
     */
    [[nodiscard]] auto handle() const noexcept -> VmaAllocator {
        return allocator_;
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
    
    // Non-copyable, movable
    Allocator(const Allocator&) = delete;
    auto operator=(const Allocator&) -> Allocator& = delete;
    
    Allocator(Allocator&& other) noexcept;
    auto operator=(Allocator&& other) noexcept -> Allocator&;
    
private:
    Allocator() = default;
    
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
};

/**
 * @class Buffer
 * @brief Vulkan buffer wrapper with VMA allocation
 * 
 * Manages VkBuffer and its memory allocation through VMA.
 * Supports staging, uniform, storage, and vertex buffers.
 * 
 * ⚠️ IMPURE CLASS (manages GPU memory)
 * 
 * @note Non-copyable, movable
 */
class Buffer {
public:
    /**
     * @brief Creates buffer
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param allocator VMA allocator
     * @param size Buffer size in bytes
     * @param usage Buffer usage flags
     * @param memory_usage VMA memory usage hint
     * @param flags Optional allocation creation flags
     * @return Result containing Buffer or error
     */
    [[nodiscard]] static auto create(
        const Allocator& allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage,
        VmaAllocationCreateFlags flags = 0
    ) -> Result<Buffer>;
    
    /**
     * @brief Creates buffer with initial data
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation + data upload)
     * 
     * @tparam T Data type
     * @param allocator VMA allocator
     * @param data Data span to upload
     * @param usage Buffer usage flags
     * @param memory_usage VMA memory usage hint
     * @return Result containing Buffer or error
     */
    template<typename T>
    [[nodiscard]] static auto create_with_data(
        const Allocator& allocator,
        std::span<const T> data,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage
    ) -> Result<Buffer> {
        const auto size = static_cast<VkDeviceSize>(data.size_bytes());
        auto buffer_result = create(allocator, size, usage, memory_usage);
        
        if (!buffer_result) {
            return std::unexpected(buffer_result.error());
        }
        
        auto& buffer = *buffer_result;
        auto map_result = buffer.map_and_write(data);
        
        if (!map_result) {
            return std::unexpected(map_result.error());
        }
        
        return std::move(buffer);
    }
    
    /**
     * @brief Destroys buffer
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Buffer();
    
    /**
     * @brief Gets VkBuffer handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan buffer handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkBuffer {
        return buffer_;
    }
    
    /**
     * @brief Gets buffer size
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Buffer size in bytes
     */
    [[nodiscard]] auto size() const noexcept -> VkDeviceSize {
        return size_;
    }
    
    /**
     * @brief Maps buffer memory
     * 
     * ⚠️ IMPURE FUNCTION (memory mapping)
     * 
     * @return Result containing mapped pointer or error
     */
    [[nodiscard]] auto map() -> Result<void*>;
    
    /**
     * @brief Unmaps buffer memory
     * 
     * ⚠️ IMPURE FUNCTION (memory unmapping)
     */
    auto unmap() -> void;
    
    /**
     * @brief Flushes buffer memory
     * 
     * ⚠️ IMPURE FUNCTION (memory synchronization)
     * 
     * @param offset Offset to flush from
     * @param size Size to flush (VK_WHOLE_SIZE for entire buffer)
     * @return Result indicating success or error
     */
    auto flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) const -> Result<void>;
    
    /**
     * @brief Invalidates buffer memory
     * 
     * ⚠️ IMPURE FUNCTION (memory synchronization)
     * 
     * @param offset Offset to invalidate from
     * @param size Size to invalidate (VK_WHOLE_SIZE for entire buffer)
     * @return Result indicating success or error
     */
    auto invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) const -> Result<void>;
    
    /**
     * @brief Maps memory, writes data, and unmaps
     * 
     * ⚠️ IMPURE FUNCTION (memory access + data transfer)
     * 
     * @tparam T Data type
     * @param data Data span to write
     * @param offset Offset in buffer
     * @return Result indicating success or error
     */
    template<typename T>
    auto map_and_write(std::span<const T> data, VkDeviceSize offset = 0) -> Result<void> {
        auto map_result = map();
        if (!map_result) {
            return std::unexpected(map_result.error());
        }
        
        void* mapped = *map_result;
        auto* dest = static_cast<u8*>(mapped) + offset;
        std::memcpy(dest, data.data(), data.size_bytes());
        
        unmap();
        return {};
    }
    
    /**
     * @brief Maps memory, reads data, and unmaps
     * 
     * ⚠️ IMPURE FUNCTION (memory access + data transfer)
     * 
     * @tparam T Data type
     * @param data Data span to read into
     * @param offset Offset in buffer
     * @return Result indicating success or error
     */
    template<typename T>
    auto map_and_read(std::span<T> data, VkDeviceSize offset = 0) -> Result<void> {
        auto map_result = map();
        if (!map_result) {
            return std::unexpected(map_result.error());
        }
        
        void* mapped = *map_result;
        auto* src = static_cast<const u8*>(mapped) + offset;
        std::memcpy(data.data(), src, data.size_bytes());
        
        unmap();
        return {};
    }
    
    // Non-copyable, movable
    Buffer(const Buffer&) = delete;
    auto operator=(const Buffer&) -> Buffer& = delete;
    
    Buffer(Buffer&& other) noexcept;
    auto operator=(Buffer&& other) noexcept -> Buffer&;
    
private:
    Buffer() = default;
    
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDeviceSize size_ = 0;
    void* mapped_data_ = nullptr;
};

/**
 * @class Image
 * @brief Vulkan image wrapper with VMA allocation
 * 
 * Manages VkImage and its memory allocation through VMA.
 * Supports 2D/3D images, mipmaps, and various formats.
 * 
 * ⚠️ IMPURE CLASS (manages GPU memory)
 * 
 * @note Non-copyable, movable
 */
class Image {
public:
    /**
     * @brief Creates image
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param allocator VMA allocator
     * @param width Image width
     * @param height Image height
     * @param format Image format
     * @param usage Image usage flags
     * @param memory_usage VMA memory usage hint
     * @param flags Optional allocation creation flags
     * @return Result containing Image or error
     */
    [[nodiscard]] static auto create(
        const Allocator& allocator,
        u32 width,
        u32 height,
        VkFormat format,
        VkImageUsageFlags usage,
        VmaMemoryUsage memory_usage,
        VmaAllocationCreateFlags flags = 0
    ) -> Result<Image>;
    
    /**
     * @brief Destroys image
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Image();
    
    /**
     * @brief Gets VkImage handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan image handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkImage {
        return image_;
    }
    
    /**
     * @brief Gets image view handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan image view handle
     */
    [[nodiscard]] auto view() const noexcept -> VkImageView {
        return view_;
    }
    
    /**
     * @brief Gets image format
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Image format
     */
    [[nodiscard]] auto format() const noexcept -> VkFormat {
        return format_;
    }
    
    /**
     * @brief Gets image extent
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Image extent (width, height, depth)
     */
    [[nodiscard]] auto extent() const noexcept -> VkExtent3D {
        return {width_, height_, 1};
    }
    
    // Non-copyable, movable
    Image(const Image&) = delete;
    auto operator=(const Image&) -> Image& = delete;
    
    Image(Image&& other) noexcept;
    auto operator=(Image&& other) noexcept -> Image&;
    
private:
    Image() = default;
    
    VkImage image_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    u32 width_ = 0;
    u32 height_ = 0;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
};

} // namespace luma::vulkan
