/**
 * @file pipeline.hpp
 * @brief Vulkan compute pipeline management for LUMA Engine
 * 
 * This file provides compute pipeline abstraction with RAII semantics,
 * descriptor set layout management, and push constant support. Designed
 * for functional composition and zero-cost abstractions uwu
 * 
 * Design decisions:
 * - Compute pipelines only (no graphics pipelines in Phase 1)
 * - Builder pattern for pipeline configuration
 * - Descriptor set layouts created with pipeline layout
 * - Push constants for small, frequently-updated data
 * - Pipeline caching for faster creation (disk cache)
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 * @version 1.0
 * 
 * @note Requires shader compiler module (SPIR-V input)
 * @note Uses C++26 features (latest standard)
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>

#include <vulkan/vulkan.h>

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luma::vulkan {

/**
 * @enum PipelineError
 * @brief Error codes for pipeline operations
 */
enum class PipelineError {
    shader_module_creation_failed,  ///< Failed to create VkShaderModule from SPIR-V
    pipeline_layout_creation_failed,  ///< Failed to create VkPipelineLayout
    pipeline_creation_failed,  ///< Failed to create VkPipeline
    invalid_spirv,  ///< SPIR-V data is invalid or corrupted
    invalid_descriptor_layout,  ///< Descriptor set layout is invalid
    pipeline_cache_load_failed,  ///< Failed to load pipeline cache from disk
    pipeline_cache_save_failed,  ///< Failed to save pipeline cache to disk
};

/**
 * @struct PushConstantRange
 * @brief Push constant range configuration
 * 
 * Defines a range of push constants accessible in shaders.
 * 
 * ✨ PURE DATA ✨
 */
struct PushConstantRange {
    VkShaderStageFlags stage_flags;  ///< Shader stages that access push constants
    u32 offset;  ///< Offset in bytes within push constant block
    u32 size;  ///< Size in bytes of push constant range
};

/**
 * @struct SpecializationConstant
 * @brief Specialization constant for shader compilation
 * 
 * Allows compile-time constants to be specified at pipeline creation.
 * Used for workgroup sizes, algorithm selection, feature toggles.
 * 
 * ✨ PURE DATA ✨
 */
struct SpecializationConstant {
    u32 constant_id;  ///< Constant ID in shader (layout(constant_id = N))
    u32 offset;  ///< Offset in data buffer
    u32 size;  ///< Size of constant (4 for int/float, 8 for double)
};

/**
 * @class ComputePipelineBuilder
 * @brief Builder for compute pipeline configuration
 * 
 * Provides fluent interface for configuring compute pipelines.
 * Follows builder pattern for clear, composable configuration.
 * 
 * ✨ PURE BUILDER (immutable, returns new builders) ✨
 * 
 * @note All methods return new builder (functional style)
 * @note Call build() to create pipeline
 * 
 * example usage:
 * @code
 * auto pipeline_result = ComputePipelineBuilder()
 *     .with_shader(spirv_code)
 *     .with_descriptor_layout(desc_layout)
 *     .with_push_constants(push_range)
 *     .build(device);
 * 
 * if (pipeline_result) {
 *     // pipeline created successfully uwu
 *     auto& pipeline = *pipeline_result;
 * }
 * @endcode
 */
class ComputePipelineBuilder {
public:
    /**
     * @brief Creates empty pipeline builder
     * 
     * ✨ PURE FUNCTION ✨
     */
    ComputePipelineBuilder() = default;
    
    /**
     * @brief Sets compute shader SPIR-V code
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param spirv SPIR-V bytecode for compute shader
     * @return New builder with shader configured
     * 
     * @note SPIR-V must be valid compute shader
     * @note Entry point defaults to "main"
     */
    [[nodiscard]] auto with_shader(std::vector<u32> spirv) const -> ComputePipelineBuilder;
    
    /**
     * @brief Sets shader entry point name
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param entry_point Entry point function name (default: "main")
     * @return New builder with entry point configured
     */
    [[nodiscard]] auto with_entry_point(std::string entry_point) const -> ComputePipelineBuilder;
    
    /**
     * @brief Adds descriptor set layout
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param layout Descriptor set layout handle
     * @return New builder with descriptor layout added
     * 
     * @note Multiple descriptor sets supported (set = 0, 1, 2, ...)
     * @note Order matters (index in vector = set number)
     */
    [[nodiscard]] auto with_descriptor_layout(VkDescriptorSetLayout layout) const -> ComputePipelineBuilder;
    
    /**
     * @brief Adds push constant range
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param range Push constant range configuration
     * @return New builder with push constant range added
     * 
     * @note Multiple ranges allowed (different offsets/stages)
     * @note Max size typically 128 bytes (check device limits)
     */
    [[nodiscard]] auto with_push_constants(PushConstantRange range) const -> ComputePipelineBuilder;
    
    /**
     * @brief Adds specialization constant
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param constant Specialization constant configuration
     * @return New builder with specialization constant added
     * 
     * @note Used for compile-time constants (workgroup size, etc.)
     * @note Data provided separately in build()
     */
    [[nodiscard]] auto with_specialization_constant(SpecializationConstant constant) const -> ComputePipelineBuilder;
    
    /**
     * @brief Sets specialization constant data
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param data Specialization constant values
     * @return New builder with specialization data configured
     * 
     * @note Must match offsets in specialization constants
     * @note Typically small (4-16 bytes)
     */
    [[nodiscard]] auto with_specialization_data(std::vector<u8> data) const -> ComputePipelineBuilder;
    
    /**
     * @brief Builds compute pipeline
     * 
     * ⚠️ IMPURE FUNCTION (creates GPU resources)
     * 
     * @param device Vulkan device
     * @return Result containing pipeline or error
     * 
     * @pre Shader SPIR-V must be set
     * @post Pipeline is ready for dispatch
     * 
     * @note Creates pipeline layout from descriptor layouts
     * @note Creates shader module from SPIR-V
     * @note Uses pipeline cache if available
     */
    [[nodiscard]] auto build(const Device& device) const -> std::expected<class ComputePipeline, PipelineError>;

private:
    std::vector<u32> spirv_;  ///< Compute shader SPIR-V bytecode
    std::string entry_point_ = "main";  ///< Shader entry point name
    std::vector<VkDescriptorSetLayout> descriptor_layouts_;  ///< Descriptor set layouts
    std::vector<PushConstantRange> push_constant_ranges_;  ///< Push constant ranges
    std::vector<SpecializationConstant> specialization_constants_;  ///< Specialization constants
    std::vector<u8> specialization_data_;  ///< Specialization constant data
};

/**
 * @class ComputePipeline
 * @brief Vulkan compute pipeline wrapper with RAII semantics
 * 
 * Manages VkPipeline and VkPipelineLayout for compute shaders.
 * Provides bind() and dispatch() operations for command recording.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources)
 * 
 * Design rationale:
 * - RAII for automatic cleanup (destructor frees GPU resources)
 * - Non-copyable (unique ownership of GPU handles)
 * - Movable (transfer ownership efficiently)
 * - Immutable after creation (pipeline state is fixed)
 * 
 * @note Create using ComputePipelineBuilder
 * @note Non-copyable, movable
 * @note Thread-safe for read-only operations (bind, dispatch)
 * 
 * example usage:
 * @code
 * // Create pipeline
 * auto pipeline = ComputePipelineBuilder()
 *     .with_shader(spirv)
 *     .with_descriptor_layout(layout)
 *     .build(device);
 * 
 * // Record commands
 * cmd.begin();
 * pipeline->bind(cmd);
 * pipeline->dispatch(cmd, 1920/8, 1080/8, 1);
 * cmd.end();
 * @endcode
 */
class ComputePipeline {
public:
    /**
     * @brief Move constructor
     * 
     * ✨ PURE (just moves handles, no side effects)
     */
    ComputePipeline(ComputePipeline&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     * 
     * ⚠️ IMPURE (destroys old resources)
     */
    auto operator=(ComputePipeline&& other) noexcept -> ComputePipeline&;
    
    /**
     * @brief Destroys pipeline and releases GPU resources
     * 
     * ⚠️ IMPURE (GPU resource deallocation)
     * 
     * @note Called automatically by RAII (destructor)
     * @note Safe to call multiple times (idempotent)
     */
    ~ComputePipeline();
    
    // Non-copyable
    ComputePipeline(const ComputePipeline&) = delete;
    auto operator=(const ComputePipeline&) = delete;
    
    /**
     * @brief Binds pipeline to command buffer
     * 
     * ⚠️ IMPURE (modifies command buffer state)
     * 
     * @param cmd_buffer Command buffer to bind to
     * 
     * @pre Command buffer must be in recording state
     * @post Pipeline is bound as compute pipeline
     * 
     * @note Must bind before dispatch
     * @note Must bind descriptor sets after this
     */
    auto bind(VkCommandBuffer cmd_buffer) const -> void;
    
    /**
     * @brief Dispatches compute work
     * 
     * ⚠️ IMPURE (records GPU commands)
     * 
     * @param cmd_buffer Command buffer to record into
     * @param group_count_x Number of workgroups in X dimension
     * @param group_count_y Number of workgroups in Y dimension
     * @param group_count_z Number of workgroups in Z dimension
     * 
     * @pre Pipeline must be bound
     * @pre Descriptor sets must be bound
     * @post Compute work is queued on GPU
     * 
     * @note Total threads = workgroups * local_size (from shader)
     * @note Example: dispatch(240, 135, 1) with local_size(8,8,1) = 1920x1080 threads
     * 
     * example:
     * @code
     * // Dispatch for 1920x1080 image with 8x8 workgroup size
     * pipeline.bind(cmd);
     * descriptor_set.bind(cmd, pipeline);
     * pipeline.dispatch(cmd, 1920/8, 1080/8, 1);
     * @endcode
     */
    auto dispatch(
        VkCommandBuffer cmd_buffer,
        u32 group_count_x,
        u32 group_count_y,
        u32 group_count_z
    ) const -> void;
    
    /**
     * @brief Updates push constants
     * 
     * ⚠️ IMPURE (modifies command buffer state)
     * 
     * @param cmd_buffer Command buffer to record into
     * @param stage_flags Shader stages that access push constants
     * @param offset Offset in bytes within push constant block
     * @param size Size in bytes of data to update
     * @param data Pointer to push constant data
     * 
     * @pre Pipeline must be bound
     * @pre Data must be valid for size bytes
     * @post Push constants are updated in GPU
     * 
     * @note Can be called multiple times per dispatch
     * @note Prefer push constants for small, frequently-updated data (<128 bytes)
     * 
     * example:
     * @code
     * struct PushData {
     *     f32 time;
     *     u32 frame_number;
     * };
     * PushData data{elapsed_time, frame_count};
     * pipeline.push_constants(cmd, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(data), &data);
     * @endcode
     */
    auto push_constants(
        VkCommandBuffer cmd_buffer,
        VkShaderStageFlags stage_flags,
        u32 offset,
        u32 size,
        const void* data
    ) const -> void;
    
    /**
     * @brief Gets Vulkan pipeline handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkPipeline handle
     * 
     * @note For advanced users only (direct Vulkan API access)
     */
    [[nodiscard]] auto handle() const -> VkPipeline { return pipeline_; }
    
    /**
     * @brief Gets Vulkan pipeline layout handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkPipelineLayout handle
     * 
     * @note Needed for descriptor set binding
     */
    [[nodiscard]] auto layout() const -> VkPipelineLayout { return layout_; }

private:
    /**
     * @brief Private constructor (use builder)
     * 
     * @param device Vulkan device
     * @param pipeline Pipeline handle
     * @param layout Pipeline layout handle
     * @param shader_module Shader module handle
     */
    ComputePipeline(
        const Device& device,
        VkPipeline pipeline,
        VkPipelineLayout layout,
        VkShaderModule shader_module
    );
    
    // ComputePipelineBuilder needs access to private constructor
    friend class ComputePipelineBuilder;
    
    const Device* device_ = nullptr;  ///< Vulkan device (non-owning reference)
    VkPipeline pipeline_ = VK_NULL_HANDLE;  ///< Vulkan pipeline handle
    VkPipelineLayout layout_ = VK_NULL_HANDLE;  ///< Vulkan pipeline layout handle
    VkShaderModule shader_module_ = VK_NULL_HANDLE;  ///< Vulkan shader module handle
};

/**
 * @class PipelineCache
 * @brief Vulkan pipeline cache for faster pipeline creation
 * 
 * Manages VkPipelineCache for caching compiled pipeline state.
 * Saves/loads cache to/from disk for persistent caching across runs.
 * 
 * ⚠️ IMPURE CLASS (manages GPU resources and disk I/O)
 * 
 * @note Significantly speeds up pipeline creation (5-10x faster)
 * @note Cache is device-specific (different GPUs have different caches)
 * @note Automatic save on destruction (persistent across runs)
 * 
 * example usage:
 * @code
 * // Create cache (loads from disk if exists)
 * auto cache = PipelineCache::create(device, "pipeline_cache.bin");
 * 
 * // Use cache in pipeline creation
 * auto pipeline = ComputePipelineBuilder()
 *     .with_shader(spirv)
 *     .build_with_cache(device, *cache);
 * 
 * // Cache automatically saved on destruction
 * @endcode
 */
class PipelineCache {
public:
    /**
     * @brief Creates pipeline cache
     * 
     * ⚠️ IMPURE FUNCTION (GPU resource allocation, disk I/O)
     * 
     * @param device Vulkan device
     * @param cache_file_path Path to cache file (optional)
     * @return Result containing PipelineCache or error
     * 
     * @note Loads cache from disk if file exists
     * @note Creates empty cache if file doesn't exist
     * @note Cache is device-specific (verify device UUID)
     */
    [[nodiscard]] static auto create(
        const Device& device,
        std::string_view cache_file_path = "pipeline_cache.bin"
    ) -> std::expected<PipelineCache, PipelineError>;
    
    /**
     * @brief Destroys pipeline cache and saves to disk
     * 
     * ⚠️ IMPURE (GPU resource deallocation, disk I/O)
     */
    ~PipelineCache();
    
    /**
     * @brief Move constructor
     */
    PipelineCache(PipelineCache&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     */
    auto operator=(PipelineCache&& other) noexcept -> PipelineCache&;
    
    // Non-copyable
    PipelineCache(const PipelineCache&) = delete;
    auto operator=(const PipelineCache&) = delete;
    
    /**
     * @brief Gets Vulkan pipeline cache handle
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return VkPipelineCache handle
     */
    [[nodiscard]] auto handle() const -> VkPipelineCache { return cache_; }
    
    /**
     * @brief Saves cache to disk
     * 
     * ⚠️ IMPURE FUNCTION (disk I/O)
     * 
     * @return Result indicating success or error
     * 
     * @note Called automatically on destruction
     * @note Can be called manually for periodic saves
     */
    [[nodiscard]] auto save() const -> std::expected<void, PipelineError>;

private:
    /**
     * @brief Private constructor (use create())
     */
    PipelineCache(
        const Device& device,
        VkPipelineCache cache,
        std::string cache_file_path
    );
    
    const Device* device_ = nullptr;  ///< Vulkan device (non-owning reference)
    VkPipelineCache cache_ = VK_NULL_HANDLE;  ///< Vulkan pipeline cache handle
    std::string cache_file_path_;  ///< Path to cache file on disk
};

} // namespace luma::vulkan
