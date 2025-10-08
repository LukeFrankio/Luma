/**
 * @file descriptor.cpp
 * @brief Implementation of Vulkan descriptor set management
 * 
 * This file implements descriptor set layouts, pools, and sets with
 * type-safe resource binding and RAII management uwu
 * 
 * @author LukeFrankio
 * @date 2025-10-08
 */

#include <luma/vulkan/descriptor.hpp>
#include <luma/core/logging.hpp>

#include <algorithm>

namespace luma::vulkan {

// ============================================================================
// DescriptorSetLayoutBuilder Implementation
// ============================================================================

auto DescriptorSetLayoutBuilder::add_binding(
    u32 binding,
    DescriptorType type,
    VkShaderStageFlags stage_flags,
    u32 count
) const -> DescriptorSetLayoutBuilder {
    auto builder = *this;  // Copy current state
    builder.bindings_.push_back(DescriptorBinding{
        .binding = binding,
        .type = type,
        .count = count,
        .stage_flags = stage_flags,
    });
    return builder;
}

auto DescriptorSetLayoutBuilder::build(const Device& device) const -> std::expected<DescriptorSetLayout, DescriptorError> {
    if (bindings_.empty()) {
        LOG_ERROR("DescriptorSetLayoutBuilder::build: No bindings added");
        return std::unexpected(DescriptorError::layout_creation_failed);
    }
    
    // Convert bindings to VkDescriptorSetLayoutBinding
    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(bindings_.size());
    
    for (const auto& binding : bindings_) {
        vk_bindings.push_back(binding.to_vk());
    }
    
    // Create descriptor set layout
    VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(vk_bindings.size()),
        .pBindings = vk_bindings.data(),
    };
    
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    const VkResult result = vkCreateDescriptorSetLayout(
        device.handle(),
        &layout_info,
        nullptr,
        &layout
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor set layout: VkResult = {}", static_cast<int>(result));
        return std::unexpected(DescriptorError::layout_creation_failed);
    }
    
    LOG_DEBUG("Created descriptor set layout successfully ({} bindings)", bindings_.size());
    
    return DescriptorSetLayout(device, layout, bindings_);
}

// ============================================================================
// DescriptorSetLayout Implementation
// ============================================================================

DescriptorSetLayout::DescriptorSetLayout(
    const Device& device,
    VkDescriptorSetLayout layout,
    std::vector<DescriptorBinding> bindings
)
    : device_(&device)
    , layout_(layout)
    , bindings_(std::move(bindings))
{
}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
    : device_(other.device_)
    , layout_(other.layout_)
    , bindings_(std::move(other.bindings_))
{
    other.device_ = nullptr;
    other.layout_ = VK_NULL_HANDLE;
}

auto DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept -> DescriptorSetLayout& {
    if (this != &other) {
        // Destroy current layout
        if (device_ && layout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_->handle(), layout_, nullptr);
        }
        
        // Move from other
        device_ = other.device_;
        layout_ = other.layout_;
        bindings_ = std::move(other.bindings_);
        
        // Reset other
        other.device_ = nullptr;
        other.layout_ = VK_NULL_HANDLE;
    }
    return *this;
}

DescriptorSetLayout::~DescriptorSetLayout() {
    if (device_ && layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_->handle(), layout_, nullptr);
        LOG_TRACE("Destroyed descriptor set layout");
    }
}

// ============================================================================
// DescriptorPool Implementation
// ============================================================================

auto DescriptorPool::create(
    const Device& device,
    u32 max_sets,
    VkDescriptorPoolCreateFlags flags
) -> std::expected<DescriptorPool, DescriptorError> {
    // Auto-calculate pool sizes for common descriptor types
    // Assume each descriptor set has on average:
    // - 1 uniform buffer
    // - 2 storage buffers
    // - 2 storage images
    // - 1 combined image sampler
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_sets * 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_sets * 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_sets * 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_sets * 1},
    };
    
    VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = flags,
        .maxSets = max_sets,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    
    VkDescriptorPool pool = VK_NULL_HANDLE;
    const VkResult result = vkCreateDescriptorPool(
        device.handle(),
        &pool_info,
        nullptr,
        &pool
    );
    
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create descriptor pool: VkResult = {}", static_cast<int>(result));
        return std::unexpected(DescriptorError::pool_creation_failed);
    }
    
    LOG_DEBUG("Created descriptor pool successfully (max sets: {})", max_sets);
    
    return DescriptorPool(device, pool);
}

DescriptorPool::DescriptorPool(const Device& device, VkDescriptorPool pool)
    : device_(&device)
    , pool_(pool)
{
}

DescriptorPool::DescriptorPool(DescriptorPool&& other) noexcept
    : device_(other.device_)
    , pool_(other.pool_)
{
    other.device_ = nullptr;
    other.pool_ = VK_NULL_HANDLE;
}

auto DescriptorPool::operator=(DescriptorPool&& other) noexcept -> DescriptorPool& {
    if (this != &other) {
        // Destroy current pool
        if (device_ && pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_->handle(), pool_, nullptr);
        }
        
        // Move from other
        device_ = other.device_;
        pool_ = other.pool_;
        
        // Reset other
        other.device_ = nullptr;
        other.pool_ = VK_NULL_HANDLE;
    }
    return *this;
}

DescriptorPool::~DescriptorPool() {
    if (device_ && pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_->handle(), pool_, nullptr);
        LOG_TRACE("Destroyed descriptor pool");
    }
}

auto DescriptorPool::allocate(const DescriptorSetLayout& layout) -> std::expected<DescriptorSet, DescriptorError> {
    VkDescriptorSetLayout vk_layout = layout.handle();
    
    VkDescriptorSetAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk_layout,
    };
    
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    const VkResult result = vkAllocateDescriptorSets(
        device_->handle(),
        &alloc_info,
        &descriptor_set
    );
    
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
            LOG_ERROR("Descriptor pool exhausted (out of memory)");
            return std::unexpected(DescriptorError::pool_exhausted);
        }
        LOG_ERROR("Failed to allocate descriptor set: VkResult = {}", static_cast<int>(result));
        return std::unexpected(DescriptorError::allocation_failed);
    }
    
    LOG_TRACE("Allocated descriptor set from pool");
    
    return DescriptorSet(*device_, descriptor_set, layout);
}

auto DescriptorPool::reset() -> void {
    const VkResult result = vkResetDescriptorPool(device_->handle(), pool_, 0);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to reset descriptor pool: VkResult = {}", static_cast<int>(result));
    } else {
        LOG_TRACE("Reset descriptor pool (freed all descriptor sets)");
    }
}

// ============================================================================
// DescriptorSet Implementation
// ============================================================================

DescriptorSet::DescriptorSet(
    const Device& device,
    VkDescriptorSet descriptor_set,
    const DescriptorSetLayout& layout
)
    : device_(&device)
    , descriptor_set_(descriptor_set)
    , layout_(&layout)
{
}

DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
    : device_(other.device_)
    , descriptor_set_(other.descriptor_set_)
    , layout_(other.layout_)
    , pending_writes_(std::move(other.pending_writes_))
    , buffer_infos_(std::move(other.buffer_infos_))
    , image_infos_(std::move(other.image_infos_))
{
    other.device_ = nullptr;
    other.descriptor_set_ = VK_NULL_HANDLE;
    other.layout_ = nullptr;
}

auto DescriptorSet::operator=(DescriptorSet&& other) noexcept -> DescriptorSet& {
    if (this != &other) {
        // Note: Descriptor sets are owned by pool, no cleanup needed here
        
        // Move from other
        device_ = other.device_;
        descriptor_set_ = other.descriptor_set_;
        layout_ = other.layout_;
        pending_writes_ = std::move(other.pending_writes_);
        buffer_infos_ = std::move(other.buffer_infos_);
        image_infos_ = std::move(other.image_infos_);
        
        // Reset other
        other.device_ = nullptr;
        other.descriptor_set_ = VK_NULL_HANDLE;
        other.layout_ = nullptr;
    }
    return *this;
}

auto DescriptorSet::bind_uniform_buffer(
    u32 binding,
    VkBuffer buffer,
    VkDeviceSize offset,
    VkDeviceSize range
) -> DescriptorSet& {
    // Add buffer info (write descriptor will reference this)
    buffer_infos_.push_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = range,
    });
    
    // Add write descriptor
    pending_writes_.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_infos_.back(),
        .pTexelBufferView = nullptr,
    });
    
    LOG_TRACE("Staged uniform buffer binding: binding={}, offset={}, range={}", binding, offset, range);
    
    return *this;
}

auto DescriptorSet::bind_storage_buffer(
    u32 binding,
    VkBuffer buffer,
    VkDeviceSize offset,
    VkDeviceSize range
) -> DescriptorSet& {
    buffer_infos_.push_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = range,
    });
    
    pending_writes_.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_infos_.back(),
        .pTexelBufferView = nullptr,
    });
    
    LOG_TRACE("Staged storage buffer binding: binding={}, offset={}, range={}", binding, offset, range);
    
    return *this;
}

auto DescriptorSet::bind_storage_image(
    u32 binding,
    VkImageView image_view,
    VkImageLayout layout
) -> DescriptorSet& {
    image_infos_.push_back(VkDescriptorImageInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = image_view,
        .imageLayout = layout,
    });
    
    pending_writes_.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &image_infos_.back(),
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    });
    
    LOG_TRACE("Staged storage image binding: binding={}", binding);
    
    return *this;
}

auto DescriptorSet::bind_sampled_image(
    u32 binding,
    VkImageView image_view,
    VkSampler sampler
) -> DescriptorSet& {
    image_infos_.push_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });
    
    pending_writes_.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_set_,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_infos_.back(),
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    });
    
    LOG_TRACE("Staged sampled image binding: binding={}", binding);
    
    return *this;
}

auto DescriptorSet::update() -> void {
    if (pending_writes_.empty()) {
        LOG_WARN("DescriptorSet::update called with no pending writes");
        return;
    }
    
    vkUpdateDescriptorSets(
        device_->handle(),
        static_cast<u32>(pending_writes_.size()),
        pending_writes_.data(),
        0,
        nullptr
    );
    
    LOG_DEBUG("Updated descriptor set ({} bindings)", pending_writes_.size());
    
    // Clear pending writes (can be called multiple times to re-bind)
    pending_writes_.clear();
    buffer_infos_.clear();
    image_infos_.clear();
}

auto DescriptorSet::bind(
    VkCommandBuffer cmd_buffer,
    VkPipelineLayout pipeline_layout,
    u32 set
) const -> void {
    vkCmdBindDescriptorSets(
        cmd_buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline_layout,
        set,
        1,
        &descriptor_set_,
        0,
        nullptr
    );
    
    LOG_TRACE("Bound descriptor set to command buffer (set={})", set);
}

} // namespace luma::vulkan
