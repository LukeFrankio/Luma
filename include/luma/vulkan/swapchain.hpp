/**
 * @file swapchain.hpp
 * @brief Vulkan swapchain management for LUMA Engine
 * 
 * This file provides swapchain creation, image acquisition, and presentation.
 * Handles surface format selection, present mode selection, and recreation
 * on window resize uwu
 * 
 * Design decisions:
 * - Prefer MAILBOX present mode (triple buffering, low latency)
 * - Fallback to FIFO (vsync, always available)
 * - Prefer SRGB color space
 * - Handle swapchain recreation on resize
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
 * @struct SwapchainSupportDetails
 * @brief Swapchain capabilities and supported formats/modes
 */
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

/**
 * @class Swapchain
 * @brief Vulkan swapchain wrapper with RAII semantics
 * 
 * Manages VkSwapchainKHR and swapchain images. Handles image acquisition
 * and presentation.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources, side effects)
 * 
 * @note Non-copyable, movable
 */
class Swapchain {
public:
    /**
     * @brief Creates Vulkan swapchain
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param device Vulkan device
     * @param surface Window surface
     * @param width Swapchain width
     * @param height Swapchain height
     * @param old_swapchain Optional old swapchain for recreation
     * @return Result containing Swapchain or error
     */
    [[nodiscard]] static auto create(
        const Device& device,
        VkSurfaceKHR surface,
        u32 width,
        u32 height,
        Swapchain* old_swapchain = nullptr
    ) -> Result<Swapchain>;
    
    /**
     * @brief Destroys Vulkan swapchain
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Swapchain();
    
    /**
     * @brief Gets VkSwapchainKHR handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan swapchain handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkSwapchainKHR {
        return swapchain_;
    }
    
    /**
     * @brief Gets swapchain images
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vector of swapchain image handles
     */
    [[nodiscard]] auto images() const noexcept -> const std::vector<VkImage>& {
        return images_;
    }
    
    /**
     * @brief Gets swapchain image views
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vector of swapchain image view handles
     */
    [[nodiscard]] auto image_views() const noexcept -> const std::vector<VkImageView>& {
        return image_views_;
    }
    
    /**
     * @brief Gets swapchain format
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Swapchain surface format
     */
    [[nodiscard]] auto format() const noexcept -> VkFormat {
        return format_;
    }
    
    /**
     * @brief Gets swapchain extent
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Swapchain extent (width, height)
     */
    [[nodiscard]] auto extent() const noexcept -> VkExtent2D {
        return extent_;
    }
    
    /**
     * @brief Acquires next swapchain image
     * 
     * ⚠️ IMPURE FUNCTION (GPU synchronization)
     * 
     * @param semaphore Semaphore to signal when image is available
     * @param fence Optional fence to signal
     * @param timeout Timeout in nanoseconds (default: UINT64_MAX)
     * @return Result containing image index or error
     * 
     * @note May return OUT_OF_DATE error if swapchain needs recreation
     */
    [[nodiscard]] auto acquire_next_image(
        VkSemaphore semaphore,
        VkFence fence = VK_NULL_HANDLE,
        u64 timeout = UINT64_MAX
    ) const -> Result<u32>;
    
    /**
     * @brief Presents swapchain image
     * 
     * ⚠️ IMPURE FUNCTION (GPU presentation)
     * 
     * @param queue Presentation queue
     * @param image_index Image index to present
     * @param wait_semaphore Semaphore to wait on before presenting
     * @return Result indicating success or error
     * 
     * @note May return OUT_OF_DATE error if swapchain needs recreation
     */
    [[nodiscard]] auto present(
        VkQueue queue,
        u32 image_index,
        VkSemaphore wait_semaphore = VK_NULL_HANDLE
    ) const -> Result<void>;
    
    /**
     * @brief Queries swapchain support details
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param device Physical device
     * @param surface Surface to query
     * @return Swapchain support details
     */
    [[nodiscard]] static auto query_swapchain_support(
        VkPhysicalDevice device,
        VkSurfaceKHR surface
    ) -> SwapchainSupportDetails;
    
    // Non-copyable, movable
    Swapchain(const Swapchain&) = delete;
    auto operator=(const Swapchain&) -> Swapchain& = delete;
    
    Swapchain(Swapchain&& other) noexcept;
    auto operator=(Swapchain&& other) noexcept -> Swapchain&;
    
private:
    Swapchain() = default;
    
    VkDevice device_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
    
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_ = {0, 0};
};

/**
 * @brief Queries swapchain support details
 * 
 * ⚠️ IMPURE FUNCTION (queries GPU)
 * 
 * @param physical_device Physical device to query
 * @param surface Surface to query support for
 * @return Swapchain support details
 */
[[nodiscard]] auto query_swapchain_support(
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface
) -> SwapchainSupportDetails;

} // namespace luma::vulkan
