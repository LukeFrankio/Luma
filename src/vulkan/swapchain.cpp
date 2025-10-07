/**
 * @file swapchain.cpp
 * @brief Implementation of Vulkan swapchain management
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/swapchain.hpp>
#include <luma/core/logging.hpp>

#include <algorithm>
#include <limits>

namespace luma::vulkan {

namespace {

/**
 * @brief Chooses best surface format
 */
auto choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available) -> VkSurfaceFormatKHR {
    // Prefer SRGB if available
    for (const auto& format : available) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    
    // Fallback to first available
    return available[0];
}

/**
 * @brief Chooses best present mode
 */
auto choose_present_mode(const std::vector<VkPresentModeKHR>& available) -> VkPresentModeKHR {
    // Prefer mailbox (triple buffering) if available
    for (const auto& mode : available) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    
    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

/**
 * @brief Chooses swap extent
 */
auto choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) -> VkExtent2D {
    // If current extent is not special value, use it
    constexpr u32 special_value = std::numeric_limits<u32>::max();
    if (capabilities.currentExtent.width != special_value) {
        return capabilities.currentExtent;
    }
    
    // Otherwise, choose extent within min/max bounds
    VkExtent2D extent = {width, height};
    
    extent.width = std::clamp(extent.width, 
                             capabilities.minImageExtent.width,
                             capabilities.maxImageExtent.width);
    
    extent.height = std::clamp(extent.height,
                              capabilities.minImageExtent.height,
                              capabilities.maxImageExtent.height);
    
    return extent;
}

} // anonymous namespace

// ============================================================================
// Swapchain Implementation
// ============================================================================

auto Swapchain::create(
    const Device& device,
    VkSurfaceKHR surface,
    u32 width,
    u32 height,
    Swapchain* old_swapchain
) -> Result<Swapchain> {
    Swapchain swapchain;
    
    LOG_INFO("Creating swapchain ({}x{})...", width, height);
    
    // Query surface capabilities
    const auto support = query_swapchain_support(device.physical_device(), surface);
    
    if (support.formats.empty() || support.present_modes.empty()) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            "Inadequate swapchain support"
        });
    }
    
    // Choose settings
    const auto surface_format = choose_surface_format(support.formats);
    const auto present_mode = choose_present_mode(support.present_modes);
    const auto extent = choose_extent(support.capabilities, width, height);
    
    // Choose image count (prefer one more than minimum for triple buffering)
    u32 image_count = support.capabilities.minImageCount + 1;
    
    if (support.capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, support.capabilities.maxImageCount);
    }
    
    LOG_INFO("  Format: {} (color space: {})", 
             static_cast<i32>(surface_format.format),
             static_cast<i32>(surface_format.colorSpace));
    LOG_INFO("  Present mode: {}", static_cast<i32>(present_mode));
    LOG_INFO("  Extent: {}x{}", extent.width, extent.height);
    LOG_INFO("  Image count: {}", image_count);
    
    // Create swapchain
    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    // Handle queue family sharing
    const auto& indices = device.queue_families();
    const u32 queue_family_indices[] = {
        *indices.graphics,
        *indices.present
    };
    
    if (indices.graphics != indices.present) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old_swapchain ? old_swapchain->swapchain_ : VK_NULL_HANDLE;
    
    const auto result = vkCreateSwapchainKHR(device.handle(), &create_info, 
                                            nullptr, &swapchain.swapchain_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create swapchain: {}", static_cast<i32>(result))
        });
    }
    
    swapchain.device_ = device.handle();
    swapchain.format_ = surface_format.format;
    swapchain.extent_ = extent;
    
    // Retrieve swapchain images
    u32 actual_image_count = 0;
    vkGetSwapchainImagesKHR(device.handle(), swapchain.swapchain_, 
                           &actual_image_count, nullptr);
    
    swapchain.images_.resize(actual_image_count);
    vkGetSwapchainImagesKHR(device.handle(), swapchain.swapchain_, 
                           &actual_image_count, swapchain.images_.data());
    
    // Create image views
    swapchain.image_views_.resize(actual_image_count);
    
    for (u32 i = 0; i < actual_image_count; ++i) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchain.images_[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = surface_format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        const auto view_result = vkCreateImageView(device.handle(), &view_info, 
                                                   nullptr, &swapchain.image_views_[i]);
        
        if (view_result != VK_SUCCESS) {
            // Cleanup already created views
            for (u32 j = 0; j < i; ++j) {
                vkDestroyImageView(device.handle(), swapchain.image_views_[j], nullptr);
            }
            
            vkDestroySwapchainKHR(device.handle(), swapchain.swapchain_, nullptr);
            
            return std::unexpected(Error{
                ErrorCode::VULKAN_INITIALIZATION_FAILED,
                std::format("Failed to create image view {}: {}", i, static_cast<i32>(view_result))
            });
        }
    }
    
    LOG_INFO("Swapchain created successfully with {} images", actual_image_count);
    
    return swapchain;
}

Swapchain::~Swapchain() {
    if (device_ != VK_NULL_HANDLE) {
        for (auto view : image_views_) {
            vkDestroyImageView(device_, view, nullptr);
        }
        
        if (swapchain_ != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            LOG_INFO("Swapchain destroyed");
        }
    }
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : device_(other.device_)
    , swapchain_(other.swapchain_)
    , images_(std::move(other.images_))
    , image_views_(std::move(other.image_views_))
    , format_(other.format_)
    , extent_(other.extent_) {
    other.swapchain_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
}

auto Swapchain::operator=(Swapchain&& other) noexcept -> Swapchain& {
    if (this != &other) {
        // Cleanup current resources
        this->~Swapchain();
        
        // Move from other
        swapchain_ = other.swapchain_;
        device_ = other.device_;
        images_ = std::move(other.images_);
        image_views_ = std::move(other.image_views_);
        format_ = other.format_;
        extent_ = other.extent_;
        
        // Nullify other
        other.swapchain_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

auto Swapchain::acquire_next_image(
    VkSemaphore semaphore,
    VkFence fence,
    u64 timeout
) const -> Result<u32> {
    u32 image_index = 0;
    
    const auto result = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        timeout,
        semaphore,
        fence,
        &image_index
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_SWAPCHAIN_OUT_OF_DATE,
            "Swapchain out of date"
        });
    }
    
    if (result == VK_SUBOPTIMAL_KHR) {
        LOG_WARN("Swapchain suboptimal, consider recreating");
    } else if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to acquire next image: {}", static_cast<i32>(result))
        });
    }
    
    return image_index;
}

auto Swapchain::present(VkQueue present_queue, u32 image_index, VkSemaphore wait_semaphore) const -> Result<void> {
    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = wait_semaphore != VK_NULL_HANDLE ? 1 : 0;
    present_info.pWaitSemaphores = &wait_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &image_index;
    
    const auto result = vkQueuePresentKHR(present_queue, &present_info);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_SWAPCHAIN_OUT_OF_DATE,
            "Swapchain out of date or suboptimal"
        });
    }
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to present: {}", static_cast<i32>(result))
        });
    }
    
    return {};
}

auto Swapchain::query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) -> SwapchainSupportDetails {
    SwapchainSupportDetails details;
    
    // Query capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    
    // Query formats
    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    
    if (format_count > 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, 
                                            details.formats.data());
    }
    
    // Query present modes
    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    
    if (present_mode_count > 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                 details.present_modes.data());
    }
    
    return details;
}

} // namespace luma::vulkan
