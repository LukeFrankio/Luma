/**
 * @file instance.hpp
 * @brief Vulkan instance management for LUMA Engine
 * 
 * This file provides RAII wrapper for VkInstance with validation layers
 * and debug messenger support. Handles instance creation, extension
 * enumeration, and debug callbacks uwu
 * 
 * Design decisions:
 * - RAII wrapper (automatic cleanup in destructor)
 * - Validation layers enabled in debug builds only
 * - Debug messenger with custom callback
 * - Extension checking before instance creation
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Requires Vulkan 1.3+
 */

#pragma once

#include <luma/core/types.hpp>

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace luma::vulkan {

/**
 * @class Instance
 * @brief Vulkan instance wrapper with RAII semantics
 * 
 * Manages VkInstance creation, validation layers, and debug messenger.
 * Automatically cleans up resources in destructor.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources, side effects)
 * 
 * @note Non-copyable, movable
 */
class Instance {
public:
    /**
     * @brief Creates Vulkan instance with validation layers
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param app_name Application name
     * @param app_version Application version (VK_MAKE_VERSION format)
     * @param enable_validation Enable validation layers (debug only)
     * @return Result containing Instance or error
     * 
     * @note Automatically enables required extensions
     */
    [[nodiscard]] static auto create(
        std::string_view app_name,
        u32 app_version,
        bool enable_validation = true
    ) -> Result<Instance>;
    
    /**
     * @brief Destroys Vulkan instance
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~Instance();
    
    /**
     * @brief Gets VkInstance handle
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vulkan instance handle
     */
    [[nodiscard]] auto handle() const noexcept -> VkInstance {
        return instance_;
    }
    
    /**
     * @brief Gets enabled validation layers
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Vector of validation layer names
     */
    [[nodiscard]] auto validation_layers() const noexcept -> const std::vector<const char*>& {
        return enabled_layers_;
    }
    
    /**
     * @brief Checks if validation is enabled
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return true if validation layers are enabled
     */
    [[nodiscard]] auto has_validation() const noexcept -> bool {
        return validation_enabled_;
    }
    
    // Non-copyable, movable
    Instance(const Instance&) = delete;
    auto operator=(const Instance&) -> Instance& = delete;
    
    Instance(Instance&& other) noexcept;
    auto operator=(Instance&& other) noexcept -> Instance&;
    
private:
    Instance() = default;
    
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    bool validation_enabled_ = false;
    std::vector<const char*> enabled_layers_;
    std::vector<const char*> enabled_extensions_;
};

} // namespace luma::vulkan
