/**
 * @file pipeline.cpp
 * @brief Implementation of Vulkan compute pipeline management
 * 
 * This file implements the compute pipeline abstraction with builder pattern,
 * pipeline caching, and RAII resource management. All functions follow
 * functional programming principles where possible uwu
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#include <luma/vulkan/pipeline.hpp>
#include <luma/core/logging.hpp>

#include <cstddef>
#include <fstream>
#include <filesystem>

namespace luma::vulkan {

// ============================================================================
// ComputePipelineBuilder Implementation
// ============================================================================

auto ComputePipelineBuilder::with_shader(std::vector<u32> spirv) const -> ComputePipelineBuilder {
    auto builder = *this;  // Copy current state
    builder.spirv_ = std::move(spirv);
    return builder;
}

auto ComputePipelineBuilder::with_entry_point(std::string entry_point) const -> ComputePipelineBuilder {
    auto builder = *this;
    builder.entry_point_ = std::move(entry_point);
    return builder;
}

auto ComputePipelineBuilder::with_descriptor_layout(VkDescriptorSetLayout layout) const -> ComputePipelineBuilder {
    auto builder = *this;
    builder.descriptor_layouts_.push_back(layout);
    return builder;
}

auto ComputePipelineBuilder::with_push_constants(PushConstantRange range) const -> ComputePipelineBuilder {
    auto builder = *this;
    builder.push_constant_ranges_.push_back(range);
    return builder;
}

auto ComputePipelineBuilder::with_specialization_constant(SpecializationConstant constant) const -> ComputePipelineBuilder {
    auto builder = *this;
    builder.specialization_constants_.push_back(constant);
    return builder;
}

auto ComputePipelineBuilder::with_specialization_data(std::vector<u8> data) const -> ComputePipelineBuilder {
    auto builder = *this;
    builder.specialization_data_ = std::move(data);
    return builder;
}

auto ComputePipelineBuilder::build(const Device& device) const -> std::expected<ComputePipeline, PipelineError> {
    // Validate SPIR-V is set
    if (spirv_.empty()) {
        LOG_ERROR("ComputePipelineBuilder::build: SPIR-V is empty");
        return std::unexpected(PipelineError::invalid_spirv);
    }
    
    // Create shader module from SPIR-V
    VkShaderModuleCreateInfo shader_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = spirv_.size() * sizeof(u32),
        .pCode = spirv_.data(),
    };
    
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(
        device.handle(),
        &shader_module_info,
        nullptr,
        &shader_module
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create shader module: VkResult = {}", static_cast<int>(result));
        return std::unexpected(PipelineError::shader_module_creation_failed);
    }
    
    LOG_DEBUG("Created shader module successfully (entry point: {})", entry_point_);
    
    // Setup specialization constants if any
    std::vector<VkSpecializationMapEntry> spec_entries;
    spec_entries.reserve(specialization_constants_.size());
    
    for (const auto& spec_const : specialization_constants_) {
        spec_entries.push_back(VkSpecializationMapEntry{
            .constantID = spec_const.constant_id,
            .offset = spec_const.offset,
            .size = spec_const.size,
        });
    }
    
    VkSpecializationInfo specialization_info{
        .mapEntryCount = static_cast<u32>(spec_entries.size()),
        .pMapEntries = spec_entries.data(),
        .dataSize = specialization_data_.size(),
        .pData = specialization_data_.data(),
    };
    
    // Create pipeline shader stage
    VkPipelineShaderStageCreateInfo shader_stage_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = entry_point_.c_str(),
        .pSpecializationInfo = spec_entries.empty() ? nullptr : &specialization_info,
    };
    
    // Convert push constant ranges to VkPushConstantRange
    std::vector<VkPushConstantRange> vk_push_ranges;
    vk_push_ranges.reserve(push_constant_ranges_.size());
    
    for (const auto& range : push_constant_ranges_) {
        vk_push_ranges.push_back(VkPushConstantRange{
            .stageFlags = range.stage_flags,
            .offset = range.offset,
            .size = range.size,
        });
    }
    
    // Create pipeline layout
    VkPipelineLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<u32>(descriptor_layouts_.size()),
        .pSetLayouts = descriptor_layouts_.data(),
        .pushConstantRangeCount = static_cast<u32>(vk_push_ranges.size()),
        .pPushConstantRanges = vk_push_ranges.data(),
    };
    
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    result = vkCreatePipelineLayout(
        device.handle(),
        &layout_info,
        nullptr,
        &pipeline_layout
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout: VkResult = {}", static_cast<int>(result));
        vkDestroyShaderModule(device.handle(), shader_module, nullptr);
        return std::unexpected(PipelineError::pipeline_layout_creation_failed);
    }
    
    LOG_DEBUG("Created pipeline layout successfully ({} descriptor sets, {} push constant ranges)",
        descriptor_layouts_.size(), vk_push_ranges.size());
    
    // Create compute pipeline
    VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shader_stage_info,
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    result = vkCreateComputePipelines(
        device.handle(),
        VK_NULL_HANDLE,  // No pipeline cache (can be added later)
        1,
        &pipeline_info,
        nullptr,
        &pipeline
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create compute pipeline: VkResult = {}", static_cast<int>(result));
        vkDestroyPipelineLayout(device.handle(), pipeline_layout, nullptr);
        vkDestroyShaderModule(device.handle(), shader_module, nullptr);
        return std::unexpected(PipelineError::pipeline_creation_failed);
    }
    
    LOG_INFO("Created compute pipeline successfully (entry: {}, descriptors: {}, push constants: {} bytes)",
        entry_point_,
        descriptor_layouts_.size(),
        vk_push_ranges.empty() ? 0 : vk_push_ranges[0].size);
    
    // Return ComputePipeline with ownership of all handles
    return ComputePipeline(device, pipeline, pipeline_layout, shader_module);
}

// ============================================================================
// ComputePipeline Implementation
// ============================================================================

ComputePipeline::ComputePipeline(
    const Device& device,
    VkPipeline pipeline,
    VkPipelineLayout layout,
    VkShaderModule shader_module
)
    : device_(&device)
    , pipeline_(pipeline)
    , layout_(layout)
    , shader_module_(shader_module)
{
}

ComputePipeline::ComputePipeline(ComputePipeline&& other) noexcept
    : device_(other.device_)
    , pipeline_(other.pipeline_)
    , layout_(other.layout_)
    , shader_module_(other.shader_module_)
{
    other.device_ = nullptr;
    other.pipeline_ = VK_NULL_HANDLE;
    other.layout_ = VK_NULL_HANDLE;
    other.shader_module_ = VK_NULL_HANDLE;
}

auto ComputePipeline::operator=(ComputePipeline&& other) noexcept -> ComputePipeline& {
    if (this != &other) {
        // Destroy current resources
        if (device_) {
            if (pipeline_ != VK_NULL_HANDLE) {
                vkDestroyPipeline(device_->handle(), pipeline_, nullptr);
            }
            if (layout_ != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device_->handle(), layout_, nullptr);
            }
            if (shader_module_ != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device_->handle(), shader_module_, nullptr);
            }
        }
        
        // Move from other
        device_ = other.device_;
        pipeline_ = other.pipeline_;
        layout_ = other.layout_;
        shader_module_ = other.shader_module_;
        
        // Reset other
        other.device_ = nullptr;
        other.pipeline_ = VK_NULL_HANDLE;
        other.layout_ = VK_NULL_HANDLE;
        other.shader_module_ = VK_NULL_HANDLE;
    }
    return *this;
}

ComputePipeline::~ComputePipeline() {
    if (device_) {
        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_->handle(), pipeline_, nullptr);
            LOG_TRACE("Destroyed compute pipeline");
        }
        if (layout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_->handle(), layout_, nullptr);
            LOG_TRACE("Destroyed pipeline layout");
        }
        if (shader_module_ != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_->handle(), shader_module_, nullptr);
            LOG_TRACE("Destroyed shader module");
        }
    }
}

auto ComputePipeline::bind(VkCommandBuffer cmd_buffer) const -> void {
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    LOG_TRACE("Bound compute pipeline to command buffer");
}

auto ComputePipeline::dispatch(
    VkCommandBuffer cmd_buffer,
    u32 group_count_x,
    u32 group_count_y,
    u32 group_count_z
) const -> void {
    vkCmdDispatch(cmd_buffer, group_count_x, group_count_y, group_count_z);
    LOG_TRACE("Dispatched compute: {}x{}x{} workgroups", group_count_x, group_count_y, group_count_z);
}

auto ComputePipeline::push_constants(
    VkCommandBuffer cmd_buffer,
    VkShaderStageFlags stage_flags,
    u32 offset,
    u32 size,
    const void* data
) const -> void {
    vkCmdPushConstants(cmd_buffer, layout_, stage_flags, offset, size, data);
    LOG_TRACE("Updated push constants: {} bytes at offset {}", size, offset);
}

// ============================================================================
// PipelineCache Implementation
// ============================================================================

auto PipelineCache::create(
    const Device& device,
    std::string_view cache_file_path
) -> std::expected<PipelineCache, PipelineError> {
    // Try to load cache from disk
    std::vector<u8> cache_data;
    
    if (std::filesystem::exists(cache_file_path)) {
        std::ifstream file(cache_file_path.data(), std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            const auto file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            cache_data.resize(static_cast<std::size_t>(file_size));
            file.read(reinterpret_cast<char*>(cache_data.data()), file_size);
            
            LOG_INFO("Loaded pipeline cache from disk: {} ({} bytes)",
                cache_file_path, cache_data.size());
        } else {
            LOG_WARN("Failed to open pipeline cache file: {}", cache_file_path);
        }
    } else {
        LOG_DEBUG("Pipeline cache file not found, creating new cache: {}", cache_file_path);
    }
    
    // Create pipeline cache
    VkPipelineCacheCreateInfo cache_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .initialDataSize = cache_data.size(),
        .pInitialData = cache_data.empty() ? nullptr : cache_data.data(),
    };
    
    VkPipelineCache cache = VK_NULL_HANDLE;
    const VkResult result = vkCreatePipelineCache(
        device.handle(),
        &cache_info,
        nullptr,
        &cache
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline cache: VkResult = {}", static_cast<int>(result));
        return std::unexpected(PipelineError::pipeline_cache_load_failed);
    }
    
    LOG_DEBUG("Created pipeline cache successfully");
    
    return PipelineCache(device, cache, std::string(cache_file_path));
}

PipelineCache::PipelineCache(
    const Device& device,
    VkPipelineCache cache,
    std::string cache_file_path
)
    : device_(&device)
    , cache_(cache)
    , cache_file_path_(std::move(cache_file_path))
{
}

PipelineCache::PipelineCache(PipelineCache&& other) noexcept
    : device_(other.device_)
    , cache_(other.cache_)
    , cache_file_path_(std::move(other.cache_file_path_))
{
    other.device_ = nullptr;
    other.cache_ = VK_NULL_HANDLE;
}

auto PipelineCache::operator=(PipelineCache&& other) noexcept -> PipelineCache& {
    if (this != &other) {
        // Save and destroy current cache
        if (device_ && cache_ != VK_NULL_HANDLE) {
            (void)save();  // Ignore errors on destruction
            vkDestroyPipelineCache(device_->handle(), cache_, nullptr);
        }
        
        // Move from other
        device_ = other.device_;
        cache_ = other.cache_;
        cache_file_path_ = std::move(other.cache_file_path_);
        
        // Reset other
        other.device_ = nullptr;
        other.cache_ = VK_NULL_HANDLE;
    }
    return *this;
}

PipelineCache::~PipelineCache() {
    if (device_ && cache_ != VK_NULL_HANDLE) {
        // Save cache to disk before destroying
        (void)save();  // Ignore errors on destruction
        
        vkDestroyPipelineCache(device_->handle(), cache_, nullptr);
        LOG_TRACE("Destroyed pipeline cache");
    }
}

auto PipelineCache::save() const -> std::expected<void, PipelineError> {
    if (!device_ || cache_ == VK_NULL_HANDLE) {
        return std::unexpected(PipelineError::pipeline_cache_save_failed);
    }
    
    // Get cache data size
    std::size_t cache_size = 0;
    VkResult result = vkGetPipelineCacheData(
        device_->handle(),
        cache_,
        &cache_size,
        nullptr
    );
    
    if (result != VK_SUCCESS || cache_size == 0) {
        LOG_ERROR("Failed to get pipeline cache size: VkResult = {}", static_cast<int>(result));
        return std::unexpected(PipelineError::pipeline_cache_save_failed);
    }
    
    // Get cache data
    std::vector<u8> cache_data(cache_size);
    result = vkGetPipelineCacheData(
        device_->handle(),
        cache_,
        &cache_size,
        cache_data.data()
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to get pipeline cache data: VkResult = {}", static_cast<int>(result));
        return std::unexpected(PipelineError::pipeline_cache_save_failed);
    }
    
    // Save to disk
    std::ofstream file(cache_file_path_, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open pipeline cache file for writing: {}", cache_file_path_);
        return std::unexpected(PipelineError::pipeline_cache_save_failed);
    }
    
    file.write(reinterpret_cast<const char*>(cache_data.data()), static_cast<std::streamsize>(cache_size));
    
    if (!file.good()) {
        LOG_ERROR("Failed to write pipeline cache to disk: {}", cache_file_path_);
        return std::unexpected(PipelineError::pipeline_cache_save_failed);
    }
    
    LOG_INFO("Saved pipeline cache to disk: {} ({} bytes)", cache_file_path_, cache_size);
    
    return {};
}

} // namespace luma::vulkan
