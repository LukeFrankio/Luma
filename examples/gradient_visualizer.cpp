/**
 * @file gradient_visualizer.cpp
 * @brief Executable to visualize gradient compute shader output (Slang edition uwu)
 * 
 * This example:
 * 1. Initializes Vulkan (instance, device, allocator)
 * 2. Compiles gradient.slang shader to SPIR-V (using Slang compiler!)
 * 3. Creates storage image and compute pipeline
 * 4. Dispatches shader to generate red-to-green gradient
 * 5. Copies image data from GPU to CPU
 * 6. Saves result as PNG file (gradient_output.png)
 * 
 * ✨ PURE FUNCTIONS + IMPERATIVE SHELL + SLANG SUPREMACY ✨
 * 
 * @author LukeFrankio
 * @date 2025-10-09
 * 
 * @note Requires Vulkan 1.3+ and compute queue support
 * @note Requires Slang compiler (slangc.exe) in PATH or build directory
 * @note Output resolution: 1920x1080 (R8G8B8A8_UNORM)
 * @note Generates gradient_output.png in current directory
 */

#include <luma/asset/shader_compiler.hpp>
#include <luma/core/logging.hpp>
#include <luma/vulkan/command_buffer.hpp>
#include <luma/vulkan/descriptor.hpp>
#include <luma/vulkan/device.hpp>
#include <luma/vulkan/instance.hpp>
#include <luma/vulkan/memory.hpp>
#include <luma/vulkan/pipeline.hpp>
#include <luma/vulkan/sync.hpp>

// Disable warnings for third-party stb library
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wuseless-cast"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#pragma GCC diagnostic pop

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace luma;
using namespace luma::vulkan;
using namespace luma::asset;

/**
 * @brief Main entry point for gradient visualizer
 * 
 * Demonstrates complete GPU compute workflow:
 * - Shader compilation and caching
 * - Compute pipeline creation
 * - GPU dispatch with synchronization
 * - Image readback and file I/O
 * 
 * ⚠️ IMPURE FUNCTION (has side effects)
 * 
 * Side effects:
 * - Creates Vulkan instance and device
 * - Allocates GPU memory
 * - Writes gradient_output.png to disk
 * - Prints logging output to console
 * 
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
auto main() -> int {
    LOG_INFO("=== Gradient Visualizer ===");
    LOG_INFO("Initializing Vulkan for compute...");
    
    // Image dimensions
    constexpr u32 WIDTH = 1920;
    constexpr u32 HEIGHT = 1080;
    constexpr VkFormat FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
    
    // Step 1: Create Vulkan instance
    auto instance_result = Instance::create("GradientVisualizer", VK_MAKE_API_VERSION(0, 1, 0, 0));
    if (!instance_result) {
        LOG_ERROR("Failed to create Vulkan instance");
        return EXIT_FAILURE;
    }
    auto instance = std::move(*instance_result);
    LOG_INFO("✓ Vulkan instance created");
    
    // Step 2: Create logical device with compute queue
    auto device_result = Device::create(instance);
    if (!device_result) {
        LOG_ERROR("Failed to create Vulkan device");
        return EXIT_FAILURE;
    }
    auto device = std::move(*device_result);
    LOG_INFO("✓ Logical device created");
    
    // Step 3: Create memory allocator
    auto allocator_result = Allocator::create(instance, device);
    if (!allocator_result) {
        LOG_ERROR("Failed to create memory allocator");
        return EXIT_FAILURE;
    }
    auto allocator = std::move(*allocator_result);
    LOG_INFO("✓ Memory allocator created");
    
    // Step 4: Get compute queue family
    auto queue_families = device.queue_families();
    if (!queue_families.compute) {
        LOG_ERROR("No compute queue family available");
        return EXIT_FAILURE;
    }
    
    // Step 5: Create command pool
    auto cmd_pool_result = CommandPool::create(
        device,
        *queue_families.compute,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );
    if (!cmd_pool_result) {
        LOG_ERROR("Failed to create command pool");
        return EXIT_FAILURE;
    }
    auto cmd_pool = std::move(*cmd_pool_result);
    LOG_INFO("✓ Command pool created");
    
    // Step 6: Create fence for synchronization
    auto fence_result = Fence::create(device.handle(), false);
    if (!fence_result) {
        LOG_ERROR("Failed to create fence");
        return EXIT_FAILURE;
    }
    auto fence = std::move(*fence_result);
    LOG_INFO("✓ Fence created");
    
    // Step 7: Compile gradient shader with Slang
    LOG_INFO("Compiling gradient.slang shader with Slang compiler...");
    // Path relative to build/bin directory (where executable runs from)
    ShaderCompiler compiler("../../shaders", "../../shaders_cache");
    auto shader_result = compiler.compile("gradient.slang", false);
    if (!shader_result) {
        LOG_ERROR("Failed to compile gradient shader");
        return EXIT_FAILURE;
    }
    const auto& shader_module = *shader_result;
    LOG_INFO("✓ Slang shader compiled: {} SPIR-V words", shader_module.spirv.size());
    
    // Step 8: Create storage image (GPU-only)
    LOG_INFO("Creating {}x{} storage image...", WIDTH, HEIGHT);
    auto image_result = Image::create(
        allocator,
        WIDTH,
        HEIGHT,
        FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0  // create flags
    );
    if (!image_result) {
        LOG_ERROR("Failed to create storage image");
        return EXIT_FAILURE;
    }
    auto image = std::move(*image_result);
    LOG_INFO("✓ Storage image created");
    
    // Step 9: Create descriptor set layout
    auto layout_result = DescriptorSetLayoutBuilder()
        .add_binding(0, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(device);
    if (!layout_result) {
        LOG_ERROR("Failed to create descriptor set layout");
        return EXIT_FAILURE;
    }
    auto descriptor_layout = std::move(*layout_result);
    LOG_INFO("✓ Descriptor set layout created");
    
    // Step 10: Create descriptor pool
    auto pool_result = DescriptorPool::create(device, 10);
    if (!pool_result) {
        LOG_ERROR("Failed to create descriptor pool");
        return EXIT_FAILURE;
    }
    auto descriptor_pool = std::move(*pool_result);
    LOG_INFO("✓ Descriptor pool created");
    
    // Step 11: Allocate descriptor set
    auto descriptor_result = descriptor_pool.allocate(descriptor_layout);
    if (!descriptor_result) {
        LOG_ERROR("Failed to allocate descriptor set");
        return EXIT_FAILURE;
    }
    auto descriptor_set = std::move(*descriptor_result);
    
    // Bind storage image to descriptor set
    descriptor_set.bind_storage_image(0, image.view(), VK_IMAGE_LAYOUT_GENERAL);
    descriptor_set.update();
    LOG_INFO("✓ Descriptor set bound to image");
    
    // Step 12: Create compute pipeline
    LOG_INFO("Creating compute pipeline...");
    auto pipeline_result = ComputePipelineBuilder()
        .with_shader(shader_module.spirv)
        .with_descriptor_layout(descriptor_layout.handle())
        .build(device);
    if (!pipeline_result) {
        LOG_ERROR("Failed to create compute pipeline");
        return EXIT_FAILURE;
    }
    auto pipeline = std::move(*pipeline_result);
    LOG_INFO("✓ Compute pipeline created");
    
    // Step 13: Allocate command buffer
    auto cmd_buffer_result = CommandBuffer::allocate(cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    if (!cmd_buffer_result) {
        LOG_ERROR("Failed to allocate command buffer");
        return EXIT_FAILURE;
    }
    auto cmd_buffer = std::move(*cmd_buffer_result);
    LOG_INFO("✓ Command buffer allocated");
    
    // Step 14: Record commands
    LOG_INFO("Recording compute commands...");
    cmd_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    // Transition image to GENERAL layout for compute shader write
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.handle();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    
    vkCmdPipelineBarrier(
        cmd_buffer.handle(),
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    // Bind pipeline and descriptor set
    pipeline.bind(cmd_buffer.handle());
    descriptor_set.bind(cmd_buffer.handle(), pipeline.layout(), 0);
    
    // Dispatch compute shader (8x8 workgroup size, so dispatch 240x135 groups)
    constexpr u32 WORKGROUP_SIZE = 8;
    const u32 dispatch_x = (WIDTH + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    const u32 dispatch_y = (HEIGHT + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    pipeline.dispatch(cmd_buffer.handle(), dispatch_x, dispatch_y, 1);
    
    LOG_INFO("✓ Dispatching {}x{} workgroups ({} total)", 
             dispatch_x, dispatch_y, dispatch_x * dispatch_y);
    
    // Transition image for transfer
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(
        cmd_buffer.handle(),
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    cmd_buffer.end();
    LOG_INFO("✓ Command buffer recorded");
    
    // Step 15: Submit and wait
    LOG_INFO("Submitting to GPU...");
    auto queue = device.compute_queue();
    
    VkCommandBuffer cmd_buffer_handle = cmd_buffer.handle();
    
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer_handle;
    
    vkQueueSubmit(queue, 1, &submit_info, fence.handle());
    
    auto wait_result = fence.wait(UINT64_MAX);
    if (!wait_result) {
        LOG_ERROR("Failed to wait for fence");
        return EXIT_FAILURE;
    }
    LOG_INFO("✓ GPU execution complete");
    
    // Step 16: Create staging buffer for readback
    LOG_INFO("Reading back image data...");
    const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(WIDTH) * HEIGHT * 4; // RGBA8
    
    auto staging_result = Buffer::create(
        allocator,
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );
    if (!staging_result) {
        LOG_ERROR("Failed to create staging buffer");
        return EXIT_FAILURE;
    }
    auto staging_buffer = std::move(*staging_result);
    
    // Step 17: Copy image to staging buffer
    fence.reset();
    cmd_buffer.reset();
    cmd_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {WIDTH, HEIGHT, 1};
    
    vkCmdCopyImageToBuffer(
        cmd_buffer.handle(),
        image.handle(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        staging_buffer.handle(),
        1,
        &region
    );
    
    cmd_buffer.end();
    
    vkQueueSubmit(queue, 1, &submit_info, fence.handle());
    wait_result = fence.wait(UINT64_MAX);
    if (!wait_result) {
        LOG_ERROR("Failed to wait for copy fence");
        return EXIT_FAILURE;
    }
    LOG_INFO("✓ Image copied to CPU");
    
    // Step 18: Map staging buffer and read data
    auto map_result = staging_buffer.map();
    if (!map_result) {
        LOG_ERROR("Failed to map staging buffer");
        return EXIT_FAILURE;
    }
    void* mapped_data = *map_result;
    
    // Step 19: Save as PNG
    LOG_INFO("Saving gradient_output.png...");
    const int result = stbi_write_png(
        "gradient_output.png",
        static_cast<int>(WIDTH),
        static_cast<int>(HEIGHT),
        4, // RGBA
        mapped_data,
        static_cast<int>(WIDTH) * 4 // stride
    );
    
    staging_buffer.unmap();
    
    if (result == 0) {
        LOG_ERROR("Failed to write PNG file");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("✓ Saved gradient_output.png ({}x{} pixels)", WIDTH, HEIGHT);
    LOG_INFO("=== Success! ===");
    LOG_INFO("Check gradient_output.png for red-to-green horizontal gradient");
    LOG_INFO("Compiled with Slang - the SUPERIOR shader language uwu ✨");
    
    return EXIT_SUCCESS;
}
