/**
 * @file descriptor.hpp
 * @brief Vulkan descriptor set management for LUMA Engine
 * 
 * This file provides descriptor set layouts, pools, and sets with RAII semantics.
 * Designed for functional composition and type-safe resource binding uwu
 * 
 * Design decisions:
 * - Descriptor pool manages allocation (pool per frame or per module)
 * - Descriptor set layout defines binding schema
 * - Descriptor sets bind actual resources (buffers, images, samplers)
 * - Builder pattern for type-safe configuration
 * - Automatic descriptor pool sizing (no manual pool management)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 * @version 1.0
 * 
 * @note Requires Vulkan device (luma/vulkan/device.hpp)
 * @note Uses C++26 features (latest standard)
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>

#include <vulkan/vulkan.h>

#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace luma::vulkan {

/**
 * @enum DescriptorError
 * @brief Error codes for descriptor operations
 */
enum class DescriptorError {
    layout_creation_failed,  ///< Failed to create descriptor set layout
    pool_creation_failed,  ///< Failed to create descriptor pool
    allocation_failed,  ///< Failed to allocate descriptor set from pool
    update_failed,  ///< Failed to update descriptor set
    invalid_binding,  ///< Binding index doesn't exist in layout
    pool_exhausted,  ///< Descriptor pool ran out of descriptors
    incompatible_type,  ///< Resource type doesn't match descriptor type
};

/**
 * @enum DescriptorType
 * @brief Types of descriptors (resources that can be bound)
 * 
 * ✨ PURE DATA ✨
 */
enum class DescriptorType {
    uniform_buffer,  ///< Uniform buffer (small, read-only)
    storage_buffer,  ///< Storage buffer (large, read-write)
    storage_image,  ///< Storage image (read-write)
    sampled_image,  ///< Sampled image (read-only, with sampler)
    sampler,  ///< Sampler object (texture filtering)
    combined_image_sampler,  ///< Combined image and sampler
};

/**
 * @brief Converts DescriptorType to VkDescriptorType
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param type DescriptorType enum value
 * @return VkDescriptorType enum value
 */
[[nodiscard]] constexpr auto to_vk_descriptor_type(DescriptorType type) -> VkDescriptorType {
    switch (type) {
        case DescriptorType::uniform_buffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::storage_buffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::storage_image:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::sampled_image:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::combined_image_sampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;  // Unreachable (all cases covered)
}

/**
 * @struct DescriptorBinding
 * @brief Descriptor binding configuration
 * 
 * Defines a single binding in descriptor set layout.
 * 
 * ✨ PURE DATA ✨
 */
struct DescriptorBinding {
    u32 binding;  ///< Binding index (layout(binding = N) in shader)
    DescriptorType type;  ///< Type of descriptor
    u32 count;  ///< Array size (1 for non-array)
    VkShaderStageFlags stage_flags;  ///< Shader stages that access this binding
    
    /**
     * @brief Converts to VkDescriptorSetLayoutBinding
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkDescriptorSetLayoutBinding structure
     */
    [[nodiscard]] auto to_vk() const -> VkDescriptorSetLayoutBinding {
        return VkDescriptorSetLayoutBinding{
            .binding = binding,
            .descriptorType = to_vk_descriptor_type(type),
            .descriptorCount = count,
            .stageFlags = stage_flags,
            .pImmutableSamplers = nullptr,
        };
    }
};

/**
 * @class DescriptorSetLayoutBuilder
 * @brief Builder for descriptor set layout configuration
 * 
 * Provides fluent interface for defining descriptor set layouts.
 * 
 * ✨ PURE BUILDER (immutable, returns new builders) ✨
 * 
 * example usage:
 * @code
 * auto layout = DescriptorSetLayoutBuilder()
 *     .add_binding(0, DescriptorType::storage_buffer, VK_SHADER_STAGE_COMPUTE_BIT)
 *     .add_binding(1, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
 *     .build(device);
 * @endcode
 */
class DescriptorSetLayoutBuilder {
public:
    /**
     * @brief Creates empty layout builder
     * 
     * ✨ PURE FUNCTION ✨
     */
    DescriptorSetLayoutBuilder() = default;
    
    /**
     * @brief Adds descriptor binding
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param binding Binding index
     * @param type Descriptor type
     * @param stage_flags Shader stages that access binding
     * @param count Array size (default: 1)
     * @return New builder with binding added
     * 
     * @note Bindings can be added in any order
     * @note Binding index must be unique within layout
     */
    [[nodiscard]] auto add_binding(
        u32 binding,
        DescriptorType type,
        VkShaderStageFlags stage_flags,
        u32 count = 1
    ) const -> DescriptorSetLayoutBuilder;
    
    /**
     * @brief Builds descriptor set layout
     * 
     * ⚠️ IMPURE FUNCTION (creates GPU resources)
     * 
     * @param device Vulkan device
     * @return Result containing layout or error
     * 
     * @pre At least one binding must be added
     */
    [[nodiscard]] auto build(const Device& device) const -> std::expected<class DescriptorSetLayout, DescriptorError>;

private:
    std::vector<DescriptorBinding> bindings_;  ///< Descriptor bindings
};

/**
 * @class DescriptorSetLayout
 * @brief Vulkan descriptor set layout wrapper with RAII semantics
 * 
 * Manages VkDescriptorSetLayout (schema for descriptor sets).
 * Defines what types of resources can be bound and at which bindings.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources)
 * 
 * @note Create using DescriptorSetLayoutBuilder
 * @note Non-copyable, movable
 */
class DescriptorSetLayout {
public:
    /**
     * @brief Move constructor
     */
    DescriptorSetLayout(DescriptorSetLayout&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     */
    auto operator=(DescriptorSetLayout&& other) noexcept -> DescriptorSetLayout&;
    
    /**
     * @brief Destroys descriptor set layout
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     */
    ~DescriptorSetLayout();
    
    // Non-copyable
    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    auto operator=(const DescriptorSetLayout&) = delete;
    
    /**
     * @brief Gets Vulkan descriptor set layout handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkDescriptorSetLayout handle
     */
    [[nodiscard]] auto handle() const -> VkDescriptorSetLayout { return layout_; }
    
    /**
     * @brief Gets descriptor bindings
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Span of descriptor bindings
     */
    [[nodiscard]] auto bindings() const -> std::span<const DescriptorBinding> {
        return bindings_;
    }

private:
    /**
     * @brief Private constructor (use builder)
     */
    DescriptorSetLayout(
        const Device& device,
        VkDescriptorSetLayout layout,
        std::vector<DescriptorBinding> bindings
    );
    
    friend class DescriptorSetLayoutBuilder;
    
    const Device* device_ = nullptr;  ///< Vulkan device (non-owning reference)
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;  ///< Vulkan descriptor set layout handle
    std::vector<DescriptorBinding> bindings_;  ///< Descriptor bindings for validation
};

/**
 * @class DescriptorPool
 * @brief Vulkan descriptor pool wrapper with RAII semantics
 * 
 * Manages VkDescriptorPool for allocating descriptor sets.
 * Pool has fixed capacity (number of descriptor sets and descriptors).
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources)
 * 
 * Design rationale:
 * - Pool per frame (reset every frame for temporary descriptors)
 * - Pool per module (persistent descriptors across frames)
 * - Auto-sizing based on usage patterns
 * 
 * @note Create using create() factory function
 * @note Non-copyable, movable
 * @note Can be reset to free all allocated descriptor sets
 * 
 * example usage:
 * @code
 * // Create pool with capacity for 100 descriptor sets
 * auto pool = DescriptorPool::create(device, 100);
 * 
 * // Allocate descriptor sets from pool
 * auto descriptor_set = pool->allocate(layout);
 * 
 * // Reset pool to free all descriptor sets (fast)
 * pool->reset();
 * @endcode
 */
class DescriptorPool {
public:
    /**
     * @brief Creates descriptor pool
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation)
     * 
     * @param device Vulkan device
     * @param max_sets Maximum number of descriptor sets to allocate
     * @param flags Descriptor pool flags (default: 0)
     * @return Result containing pool or error
     * 
     * @note Pool sizes are auto-calculated based on max_sets
     * @note Use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to allow individual frees
     */
    [[nodiscard]] static auto create(
        const Device& device,
        u32 max_sets,
        VkDescriptorPoolCreateFlags flags = 0
    ) -> std::expected<DescriptorPool, DescriptorError>;
    
    /**
     * @brief Destroys descriptor pool
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     * 
     * @note Automatically frees all allocated descriptor sets
     */
    ~DescriptorPool();
    
    /**
     * @brief Move constructor
     */
    DescriptorPool(DescriptorPool&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     */
    auto operator=(DescriptorPool&& other) noexcept -> DescriptorPool&;
    
    // Non-copyable
    DescriptorPool(const DescriptorPool&) = delete;
    auto operator=(const DescriptorPool&) = delete;
    
    /**
     * @brief Allocates descriptor set from pool
     * 
     * ⚠️ IMPURE FUNCTION (allocates from pool)
     * 
     * @param layout Descriptor set layout
     * @return Result containing descriptor set or error
     * 
     * @note Descriptor set is owned by pool (freed on pool destruction or reset)
     * @note Fails if pool is exhausted (no more capacity)
     */
    [[nodiscard]] auto allocate(const DescriptorSetLayout& layout) -> std::expected<class DescriptorSet, DescriptorError>;
    
    /**
     * @brief Resets descriptor pool
     * 
     * ⚠️ IMPURE (frees all allocated descriptor sets)
     * 
     * @note All descriptor sets allocated from this pool become invalid
     * @note Fast operation (resets internal allocator)
     * @note Use for per-frame descriptor pools
     */
    auto reset() -> void;
    
    /**
     * @brief Gets Vulkan descriptor pool handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkDescriptorPool handle
     */
    [[nodiscard]] auto handle() const -> VkDescriptorPool { return pool_; }

private:
    /**
     * @brief Private constructor (use create())
     */
    DescriptorPool(const Device& device, VkDescriptorPool pool);
    
    const Device* device_ = nullptr;  ///< Vulkan device (non-owning reference)
    VkDescriptorPool pool_ = VK_NULL_HANDLE;  ///< Vulkan descriptor pool handle
};

/**
 * @class DescriptorSet
 * @brief Vulkan descriptor set wrapper with type-safe resource binding
 * 
 * Manages VkDescriptorSet (binding of actual resources to layout).
 * Provides type-safe methods for binding buffers, images, and samplers.
 * 
 * ⚠️ IMPURE CLASS (modifies descriptor set state)
 * 
 * @note Allocated from DescriptorPool
 * @note Non-copyable, movable
 * @note Must call update() after binding resources
 * 
 * example usage:
 * @code
 * auto descriptor_set = pool.allocate(layout);
 * 
 * descriptor_set->bind_buffer(0, buffer, 0, buffer_size);
 * descriptor_set->bind_image(1, image_view, VK_IMAGE_LAYOUT_GENERAL);
 * descriptor_set->update();  // Apply bindings to GPU
 * 
 * // Bind to command buffer
 * descriptor_set->bind(cmd_buffer, pipeline);
 * @endcode
 */
class DescriptorSet {
public:
    /**
     * @brief Move constructor
     */
    DescriptorSet(DescriptorSet&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     */
    auto operator=(DescriptorSet&& other) noexcept -> DescriptorSet&;
    
    /**
     * @brief Destructor
     * 
     * @note Descriptor set is owned by pool (freed on pool destruction/reset)
     * @note No explicit cleanup needed
     */
    ~DescriptorSet() = default;
    
    // Non-copyable
    DescriptorSet(const DescriptorSet&) = delete;
    auto operator=(const DescriptorSet&) = delete;
    
    /**
     * @brief Binds uniform buffer to descriptor set
     * 
     * ⚠️ IMPURE (modifies descriptor set)
     * 
     * @param binding Binding index
     * @param buffer Buffer handle
     * @param offset Offset in bytes within buffer
     * @param range Size in bytes of buffer region
     * @return Reference to self for chaining
     * 
     * @pre Binding must exist in layout and be uniform_buffer type
     * @post Binding is staged for update (call update() to apply)
     * 
     * @note Supports method chaining
     * @note Must call update() after all bindings
     */
    auto bind_uniform_buffer(
        u32 binding,
        VkBuffer buffer,
        VkDeviceSize offset,
        VkDeviceSize range
    ) -> DescriptorSet&;
    
    /**
     * @brief Binds storage buffer to descriptor set
     * 
     * ⚠️ IMPURE (modifies descriptor set)
     * 
     * @param binding Binding index
     * @param buffer Buffer handle
     * @param offset Offset in bytes within buffer
     * @param range Size in bytes of buffer region
     * @return Reference to self for chaining
     * 
     * @pre Binding must exist in layout and be storage_buffer type
     * @note Storage buffers are read-write in shaders
     */
    auto bind_storage_buffer(
        u32 binding,
        VkBuffer buffer,
        VkDeviceSize offset,
        VkDeviceSize range
    ) -> DescriptorSet&;
    
    /**
     * @brief Binds storage image to descriptor set
     * 
     * ⚠️ IMPURE (modifies descriptor set)
     * 
     * @param binding Binding index
     * @param image_view Image view handle
     * @param layout Image layout (e.g., VK_IMAGE_LAYOUT_GENERAL)
     * @return Reference to self for chaining
     * 
     * @pre Binding must exist in layout and be storage_image type
     * @note Storage images are read-write in shaders
     * @note Image must be in specified layout before shader access
     */
    auto bind_storage_image(
        u32 binding,
        VkImageView image_view,
        VkImageLayout layout
    ) -> DescriptorSet&;
    
    /**
     * @brief Binds sampled image to descriptor set
     * 
     * ⚠️ IMPURE (modifies descriptor set)
     * 
     * @param binding Binding index
     * @param image_view Image view handle
     * @param sampler Sampler handle
     * @return Reference to self for chaining
     * 
     * @pre Binding must exist in layout and be combined_image_sampler type
     * @note Sampled images are read-only in shaders
     * @note Requires sampler for texture filtering
     */
    auto bind_sampled_image(
        u32 binding,
        VkImageView image_view,
        VkSampler sampler
    ) -> DescriptorSet&;
    
    /**
     * @brief Updates descriptor set (applies all bindings)
     * 
     * ⚠️ IMPURE (writes to GPU)
     * 
     * @note Must call after all bind_*() operations
     * @note All bindings are applied atomically
     * @note Can be called multiple times (re-bind resources)
     */
    auto update() -> void;
    
    /**
     * @brief Binds descriptor set to command buffer
     * 
     * ⚠️ IMPURE (modifies command buffer state)
     * 
     * @param cmd_buffer Command buffer to bind to
     * @param pipeline_layout Pipeline layout handle
     * @param set Set number (default: 0)
     * 
     * @pre Command buffer must be in recording state
     * @pre Pipeline must be bound
     * @post Descriptor set is bound for subsequent draw/dispatch commands
     * 
     * example:
     * @code
     * pipeline.bind(cmd);
     * descriptor_set.bind(cmd, pipeline.layout());
     * pipeline.dispatch(cmd, 240, 135, 1);
     * @endcode
     */
    auto bind(
        VkCommandBuffer cmd_buffer,
        VkPipelineLayout pipeline_layout,
        u32 set = 0
    ) const -> void;
    
    /**
     * @brief Gets Vulkan descriptor set handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkDescriptorSet handle
     */
    [[nodiscard]] auto handle() const -> VkDescriptorSet { return descriptor_set_; }

private:
    /**
     * @brief Private constructor (allocated from pool)
     */
    DescriptorSet(
        const Device& device,
        VkDescriptorSet descriptor_set,
        const DescriptorSetLayout& layout
    );
    
    friend class DescriptorPool;
    
    const Device* device_ = nullptr;  ///< Vulkan device (non-owning reference)
    VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;  ///< Vulkan descriptor set handle
    const DescriptorSetLayout* layout_ = nullptr;  ///< Descriptor set layout (non-owning reference)
    
    // Pending descriptor writes (applied on update())
    std::vector<VkWriteDescriptorSet> pending_writes_;  ///< Pending descriptor writes
    std::vector<VkDescriptorBufferInfo> buffer_infos_;  ///< Buffer info storage (referenced by writes)
    std::vector<VkDescriptorImageInfo> image_infos_;  ///< Image info storage (referenced by writes)
};

} // namespace luma::vulkan
