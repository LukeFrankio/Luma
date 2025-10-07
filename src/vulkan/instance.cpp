/**
 * @file instance.cpp
 * @brief Implementation of Vulkan instance management
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/vulkan/instance.hpp>
#include <luma/core/logging.hpp>

#include <vector>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#  define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

namespace luma::vulkan {

namespace {

// Validation layers to enable
constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

// Required instance extensions
const std::vector<const char*> REQUIRED_EXTENSIONS = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
    "VK_KHR_win32_surface",
#elif defined(__linux__)
    "VK_KHR_xlib_surface",
#endif
};

/**
 * @brief Debug callback for validation layers
 */
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    (void)type;
    (void)user_data;
    
    // Map Vulkan severity to our log levels
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR("[Vulkan] {}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("[Vulkan] {}", callback_data->pMessage);
    } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        LOG_INFO("[Vulkan] {}", callback_data->pMessage);
    } else {
        LOG_TRACE("[Vulkan] {}", callback_data->pMessage);
    }
    
    return VK_FALSE;
}

/**
 * @brief Checks if validation layers are available
 */
auto check_validation_layer_support() -> bool {
    u32 layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    
    for (const auto& layer : available_layers) {
        if (std::strcmp(layer.layerName, VALIDATION_LAYER) == 0) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Checks if all required extensions are available
 */
auto check_extension_support(const std::vector<const char*>& required) -> bool {
    u32 extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, 
                                          available_extensions.data());
    
    for (const auto* required_ext : required) {
        bool found = false;
        for (const auto& available : available_extensions) {
            if (std::strcmp(required_ext, available.extensionName) == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            LOG_ERROR("Required extension not available: {}", required_ext);
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Creates debug messenger
 */
auto create_debug_messenger(VkInstance instance) -> Result<VkDebugUtilsMessengerEXT> {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    
    // Load function pointer
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    
    if (!func) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            "Failed to load vkCreateDebugUtilsMessengerEXT"
        });
    }
    
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    const auto result = func(instance, &create_info, nullptr, &messenger);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create debug messenger: {}", static_cast<i32>(result))
        });
    }
    
    return messenger;
}

} // anonymous namespace

// ============================================================================
// Instance Implementation
// ============================================================================

auto Instance::create(
    std::string_view app_name,
    u32 app_version,
    bool enable_validation
) -> Result<Instance> {
    Instance instance;
    
    LOG_INFO("Creating Vulkan instance...");
    LOG_INFO("  Application: {} (version {})", app_name, app_version);
    
    // Check validation layer support
    instance.validation_enabled_ = enable_validation;
    
    if (instance.validation_enabled_) {
        if (!check_validation_layer_support()) {
            LOG_WARN("Validation layers requested but not available, disabling");
            instance.validation_enabled_ = false;
        } else {
            instance.enabled_layers_.push_back(VALIDATION_LAYER);
            LOG_INFO("  Validation layers: enabled");
        }
    }
    
    // Build extension list
    instance.enabled_extensions_ = REQUIRED_EXTENSIONS;
    
    if (instance.validation_enabled_) {
        instance.enabled_extensions_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Check extension support
    if (!check_extension_support(instance.enabled_extensions_)) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            "Required Vulkan extensions not available"
        });
    }
    
    LOG_INFO("  Extensions: {} enabled", instance.enabled_extensions_.size());
    for (const auto* ext : instance.enabled_extensions_) {
        LOG_TRACE("    - {}", ext);
    }
    
    // Application info
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name.data();
    app_info.applicationVersion = app_version;
    app_info.pEngineName = "LUMA Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    
    // Instance create info
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(instance.enabled_extensions_.size());
    create_info.ppEnabledExtensionNames = instance.enabled_extensions_.data();
    
    if (instance.validation_enabled_) {
        create_info.enabledLayerCount = static_cast<u32>(instance.enabled_layers_.size());
        create_info.ppEnabledLayerNames = instance.enabled_layers_.data();
    }
    
    // Create instance
    const auto result = vkCreateInstance(&create_info, nullptr, &instance.instance_);
    
    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_INITIALIZATION_FAILED,
            std::format("Failed to create Vulkan instance: {}", static_cast<i32>(result))
        });
    }
    
    LOG_INFO("Vulkan instance created successfully");
    
    // Create debug messenger if validation enabled
    if (instance.validation_enabled_) {
        auto messenger_result = create_debug_messenger(instance.instance_);
        if (!messenger_result) {
            LOG_WARN("Failed to create debug messenger: {}", messenger_result.error().what());
        } else {
            instance.debug_messenger_ = *messenger_result;
            LOG_INFO("Debug messenger created");
        }
    }
    
    return instance;
}

Instance::~Instance() {
    if (debug_messenger_ != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT")
        );
        
        if (func) {
            func(instance_, debug_messenger_, nullptr);
        }
    }
    
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        LOG_INFO("Vulkan instance destroyed");
    }
}

Instance::Instance(Instance&& other) noexcept
    : instance_(other.instance_)
    , debug_messenger_(other.debug_messenger_)
    , validation_enabled_(other.validation_enabled_)
    , enabled_layers_(std::move(other.enabled_layers_))
    , enabled_extensions_(std::move(other.enabled_extensions_)) {
    other.instance_ = VK_NULL_HANDLE;
    other.debug_messenger_ = VK_NULL_HANDLE;
}

auto Instance::operator=(Instance&& other) noexcept -> Instance& {
    if (this != &other) {
        // Cleanup current resources
        this->~Instance();
        
        // Move from other
        instance_ = other.instance_;
        debug_messenger_ = other.debug_messenger_;
        validation_enabled_ = other.validation_enabled_;
        enabled_layers_ = std::move(other.enabled_layers_);
        enabled_extensions_ = std::move(other.enabled_extensions_);
        
        // Nullify other
        other.instance_ = VK_NULL_HANDLE;
        other.debug_messenger_ = VK_NULL_HANDLE;
    }
    
    return *this;
}

} // namespace luma::vulkan
