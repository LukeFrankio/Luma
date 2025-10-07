/**
 * @file device.hpp
 * @brief Vulkan device management for LUMA Engine
 * 
 * This file provides physical and logical device management with queue
 * family selection, feature enablement, and device properties query uwu
 * 
 * Design decisions:
 * - Prefer discrete GPU, fallback to integrated GPU
 * - Require Vulkan 1.3 features (dynamic rendering, synchronization2)
 * - Query queue families (graphics, compute, transfer)
 * - Enable all available features by default (maximum compatibility)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Requires Vulkan 1.3+
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/instance.hpp>

#include <vulkan/vulkan.h>

#include <optional>
#include <vector>

namespace luma::vulkan {

/**
 * @struct QueueFamilyIndices
 * @brief Queue family indices for different queue types
 * 
 * Stores indices for graphics, compute, and transfer queues.
 * Some indices may be the same if queues share families.
 */
struct QueueFamilyIndices {
    std::optional<u32> graphics;  ///< Graphics queue family index
    std::optional<u32> compute;   ///< Compute queue family index
    std::optional<u32> transfer;  ///< Transfer queue family index
    std::optional<u32> present;   ///< Present queue family index
    
    /**
     * @brief Checks if all required queues are available
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if graphics, compute, and transfer queues are available
     */
    [[nodiscard]] auto is_complete() const -> bool;
    
    /**
     * @brief Checks if all required queues including present are available
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if all queues including present are available
     */
    [[nodiscard]] auto is_complete_with_present() const -> bool;
    
    /**
     * @brief Gets unique queue family indices
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Vector of unique queue family indices
     */
    [[nodiscard]] auto get_unique_families() const -> std::vector<u32>;
};

/**
 * @class Device
 * @brief Vulkan device wrapper with RAII semantics
 * 
 * Manages VkPhysicalDevice selection and VkDevice creation.
 * Queries device properties, features, and queue families.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources, side effects)
 * 
 * @note Non-copyable, movable
 */
class Device {
public:
    /**
     * @brief Creates Vulkan device from instance
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param instance Vulkan instance
     * @param surface Optional surface for presentation support
     * @param required_extensions List of required device extensions
     * @return Result containing Device or error
     * 
     * @note Automatically selects best physical device
     * @note Enables all available Vulkan 1.3 features
     */
    [[nodiscard]] static auto create(
        const Instance& instance,
        VkSurfaceKHR surface = VK_NULL_HANDLE,
        const std::vector<const char*>& required_extensions = {}
    ) -> Result<Device>;
    
    /**
     * @brief Destroys Vulkan device
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Device();
    
    /**
     * @brief Gets VkDevice handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan logical device handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkDevice {
        return device_;
    }
    
    /**
     * @brief Gets VkPhysicalDevice handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan physical device handle
     */
    [[nodiscard]] auto physical_device() const noexcept -> VkPhysicalDevice {
        return physical_device_;
    }
    
    /**
     * @brief Gets graphics queue handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Graphics queue handle
     */
    [[nodiscard]] auto graphics_queue() const noexcept -> VkQueue {
        return graphics_queue_;
    }
    
    /**
     * @brief Gets compute queue handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Compute queue handle
     */
    [[nodiscard]] auto compute_queue() const noexcept -> VkQueue {
        return compute_queue_;
    }
    
    /**
     * @brief Gets transfer queue handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Transfer queue handle (may be same as graphics)
     */
    [[nodiscard]] auto transfer_queue() const noexcept -> VkQueue {
        return transfer_queue_;
    }
    
    /**
     * @brief Gets queue family indices
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Queue family indices structure
     */
    [[nodiscard]] auto queue_families() const noexcept -> const QueueFamilyIndices& {
        return queue_families_;
    }
    
    /**
     * @brief Gets device properties
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan physical device properties
     */
    [[nodiscard]] auto properties() const noexcept -> const VkPhysicalDeviceProperties& {
        return properties_;
    }
    
    /**
     * @brief Waits for device to become idle
     * 
     * ⚠️ IMPURE FUNCTION (GPU synchronization)
     * 
     * @return Result indicating success or error
     */
    auto wait_idle() const -> Result<void>;
    
    // Non-copyable, movable
    Device(const Device&) = delete;
    auto operator=(const Device&) -> Device& = delete;
    
    Device(Device&& other) noexcept;
    auto operator=(Device&& other) noexcept -> Device&;
    
private:
    Device() = default;
    
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue transfer_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    
    QueueFamilyIndices queue_families_;
    VkPhysicalDeviceProperties properties_;
    VkPhysicalDeviceFeatures features_;
};

} // namespace luma::vulkan
