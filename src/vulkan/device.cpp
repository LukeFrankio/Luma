/**
 * @file device.cpp
 * @brief Implementation of Vulkan device management
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/device.hpp>
#include <luma/core/logging.hpp>

#include <vector>
#include <set>
#include <algorithm>
#include <cstring>

namespace luma::vulkan {

namespace {

/**
 * @brief Scores a physical device for suitability
 * 
 * @param device Physical device to score
 * @return Score (higher is better, 0 means unsuitable)
 */
auto score_device(VkPhysicalDevice device) -> u32 {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    
    u32 score = 0;
    
    // Prefer discrete GPU
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    
    // Integrated GPU is acceptable
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 500;
    }
    
    // Maximum image dimension adds to score
    score += properties.limits.maxImageDimension2D;
    
    LOG_TRACE("Device: {} (score: {})", properties.deviceName, score);
    
    return score;
}

/**
 * @brief Finds queue family indices for a physical device
 */
auto find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface) -> QueueFamilyIndices {
    QueueFamilyIndices indices;
    
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 
                                            queue_families.data());
    
    for (u32 i = 0; i < queue_family_count; ++i) {
        const auto& family = queue_families[i];
        
        // Graphics queue
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        
        // Compute queue (prefer dedicated, but fallback to graphics+compute)
        if (family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            if (!indices.compute.has_value()) {
                indices.compute = i;
            }
            
            // Prefer dedicated compute queue (no graphics)
            if (!(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                indices.compute = i;
            }
        }
        
        // Transfer queue (prefer dedicated)
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!indices.transfer.has_value()) {
                indices.transfer = i;
            }
            
            // Prefer dedicated transfer queue (no graphics/compute)
            constexpr auto compute_graphics_bits = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
            if (!(family.queueFlags & compute_graphics_bits)) {
                indices.transfer = i;
            }
        }
        
        // Present queue (check surface support)
        if (surface != VK_NULL_HANDLE) {
            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
            
            if (present_support) {
                indices.present = i;
            }
        }
    }
    
    return indices;
}

} // anonymous namespace

// ============================================================================
// QueueFamilyIndices Implementation
// ============================================================================

auto QueueFamilyIndices::is_complete() const -> bool {
    return graphics.has_value() && 
           compute.has_value() && 
           transfer.has_value();
}

auto QueueFamilyIndices::is_complete_with_present() const -> bool {
    return is_complete() && present.has_value();
}

auto QueueFamilyIndices::get_unique_families() const -> std::vector<u32> {
    std::set<u32> unique_families;
    
    if (graphics.has_value()) unique_families.insert(*graphics);
    if (compute.has_value()) unique_families.insert(*compute);
    if (transfer.has_value()) unique_families.insert(*transfer);
    if (present.has_value()) unique_families.insert(*present);
    
    return std::vector<u32>(unique_families.begin(), unique_families.end());
}

// ============================================================================
// Device Implementation
// ============================================================================

auto Device::create(
    const Instance& instance,
    VkSurfaceKHR surface,
    const std::vector<const char*>& required_extensions
) -> Result<Device> {
    Device device;
    
    LOG_INFO("Creating Vulkan device...");
    
    // Enumerate physical devices
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, nullptr);
    
    if (device_count == 0) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            "No Vulkan-capable devices found"
        });
    }
    
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance.handle(), &device_count, devices.data());
    
    LOG_INFO("  Found {} physical device(s)", device_count);
    
    // Score and select best device
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    u32 best_score = 0;
    QueueFamilyIndices best_indices;
    
    for (const auto& candidate : devices) {
        auto indices = find_queue_families(candidate, surface);
        
        // Check if device supports required queue families
        const bool has_required_queues = surface == VK_NULL_HANDLE 
            ? indices.is_complete() 
            : indices.is_complete_with_present();
        
        if (!has_required_queues) {
            continue;
        }
        
        // Check extension support
        u32 extension_count = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr);
        
        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, 
                                            available_extensions.data());
        
        bool all_extensions_supported = true;
        for (const auto* required_ext : required_extensions) {
            bool found = false;
            for (const auto& available : available_extensions) {
                if (std::strcmp(required_ext, available.extensionName) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                all_extensions_supported = false;
                break;
            }
        }
        
        if (!all_extensions_supported) {
            continue;
        }
        
        // Score device
        const u32 score = score_device(candidate);
        if (score > best_score) {
            best_device = candidate;
            best_score = score;
            best_indices = indices;
        }
    }
    
    if (best_device == VK_NULL_HANDLE) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            "No suitable physical device found"
        });
    }
    
    device.physical_device_ = best_device;
    device.queue_families_ = best_indices;
    
    // Log selected device
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(best_device, &properties);
    LOG_INFO("  Selected device: {}", properties.deviceName);
    LOG_INFO("  Vulkan API version: {}.{}.{}", 
             VK_VERSION_MAJOR(properties.apiVersion),
             VK_VERSION_MINOR(properties.apiVersion),
             VK_VERSION_PATCH(properties.apiVersion));
    
    // Create logical device
    const auto unique_families = best_indices.get_unique_families();
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_families.size());
    
    constexpr f32 queue_priority = 1.0f;
    
    for (const u32 family : unique_families) {
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_info);
    }
    
    // Enable Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features_13 = {};
    features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features_13.synchronization2 = VK_TRUE;
    features_13.dynamicRendering = VK_TRUE;
    
    VkPhysicalDeviceVulkan12Features features_12 = {};
    features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features_12.pNext = &features_13;
    features_12.bufferDeviceAddress = VK_TRUE;
    features_12.descriptorIndexing = VK_TRUE;
    
    VkPhysicalDeviceFeatures2 device_features = {};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &features_12;
    
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &device_features;
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<u32>(required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    
    // Enable validation layers on device (for compatibility with older implementations)
    if (instance.has_validation()) {
        const auto& layers = instance.validation_layers();
        create_info.enabledLayerCount = static_cast<u32>(layers.size());
        create_info.ppEnabledLayerNames = layers.data();
    }
    
    const auto result = vkCreateDevice(best_device, &create_info, nullptr, &device.device_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create logical device: {}", static_cast<i32>(result))
        });
    }
    
    LOG_INFO("Logical device created successfully");
    
    // Get queue handles
    vkGetDeviceQueue(device.device_, *best_indices.graphics, 0, &device.graphics_queue_);
    vkGetDeviceQueue(device.device_, *best_indices.compute, 0, &device.compute_queue_);
    vkGetDeviceQueue(device.device_, *best_indices.transfer, 0, &device.transfer_queue_);
    
    if (best_indices.present.has_value()) {
        vkGetDeviceQueue(device.device_, *best_indices.present, 0, &device.present_queue_);
    }
    
    LOG_INFO("  Queue families:");
    LOG_INFO("    Graphics: {}", *best_indices.graphics);
    LOG_INFO("    Compute:  {}", *best_indices.compute);
    LOG_INFO("    Transfer: {}", *best_indices.transfer);
    if (best_indices.present.has_value()) {
        LOG_INFO("    Present:  {}", *best_indices.present);
    }
    
    return device;
}

Device::~Device() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        LOG_INFO("Vulkan device destroyed");
    }
}

Device::Device(Device&& other) noexcept
    : device_(other.device_)
    , physical_device_(other.physical_device_)
    , graphics_queue_(other.graphics_queue_)
    , compute_queue_(other.compute_queue_)
    , transfer_queue_(other.transfer_queue_)
    , present_queue_(other.present_queue_)
    , queue_families_(other.queue_families_) {
    other.device_ = VK_NULL_HANDLE;
    other.physical_device_ = VK_NULL_HANDLE;
    other.graphics_queue_ = VK_NULL_HANDLE;
    other.compute_queue_ = VK_NULL_HANDLE;
    other.transfer_queue_ = VK_NULL_HANDLE;
    other.present_queue_ = VK_NULL_HANDLE;
}

auto Device::operator=(Device&& other) noexcept -> Device& {
    if (this != &other) {
        // Cleanup current resources
        this->~Device();
        
        // Move from other
        device_ = other.device_;
        physical_device_ = other.physical_device_;
        graphics_queue_ = other.graphics_queue_;
        compute_queue_ = other.compute_queue_;
        transfer_queue_ = other.transfer_queue_;
        present_queue_ = other.present_queue_;
        queue_families_ = other.queue_families_;
        
        // Nullify other
        other.device_ = VK_NULL_HANDLE;
        other.physical_device_ = VK_NULL_HANDLE;
        other.graphics_queue_ = VK_NULL_HANDLE;
        other.compute_queue_ = VK_NULL_HANDLE;
        other.transfer_queue_ = VK_NULL_HANDLE;
        other.present_queue_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

auto Device::wait_idle() const -> Result<void> {
    vkDeviceWaitIdle(device_);
    return {};
}

} // namespace luma::vulkan
