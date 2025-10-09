/**
 * @file test_gradient_compute.cpp
 * @brief Test gradient compute shader with pipeline and descriptor system (Slang edition uwu)
 * 
 * This test verifies the complete compute pipeline workflow:
 * - Shader compilation (Slang → SPIR-V using Slang compiler)
 * - Pipeline creation with descriptor layouts
 * - Descriptor set allocation and binding
 * - Storage image creation
 * - Compute shader dispatch
 * - Command buffer recording and submission
 * 
 * @author LukeFrankio
 * @date 2025-10-08
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

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace luma;
using namespace luma::vulkan;
using namespace luma::asset;

/**
 * @class GradientComputeTest
 * @brief Test fixture for gradient compute shader tests
 * 
 * Sets up Vulkan instance, device, and allocator for compute tests.
 * Provides helper methods for common operations.
 */
class GradientComputeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create Vulkan instance (no validation layers for speed)
        auto instance_result = Instance::create("GradientComputeTest", 1, false);
        ASSERT_TRUE(instance_result.has_value()) << "Failed to create Vulkan instance";
        instance_ = std::move(*instance_result);
        
        // Create device
        auto device_result = Device::create(*instance_);
        ASSERT_TRUE(device_result.has_value()) << "Failed to create Vulkan device";
        device_ = std::move(*device_result);
        
        // Create VMA allocator
        auto allocator_result = Allocator::create(*instance_, *device_);
        ASSERT_TRUE(allocator_result.has_value()) << "Failed to create VMA allocator";
        allocator_ = std::move(*allocator_result);
        
        // Create command pool
        auto queue_families = device_->queue_families();
        ASSERT_TRUE(queue_families.compute.has_value()) << "Compute queue family not available";
        
        auto cmd_pool_result = CommandPool::create(
            *device_,
            *queue_families.compute,
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        );
        ASSERT_TRUE(cmd_pool_result.has_value()) << "Failed to create command pool";
        cmd_pool_ = std::move(*cmd_pool_result);
        
        // Create fence for GPU synchronization
        auto fence_result = Fence::create(device_->handle(), false);
        ASSERT_TRUE(fence_result.has_value()) << "Failed to create fence";
        fence_ = std::move(*fence_result);
    }
    
    /**
     * @brief Compiles gradient.slang shader to SPIR-V (using Slang compiler uwu)
     * 
     * @return SPIR-V bytecode or empty vector on failure
     */
    auto compile_gradient_shader() -> std::vector<u32> {
        // Path relative to build directory (where tests run from)
        const std::filesystem::path shader_path = "../shaders/gradient.slang";
        
        if (!std::filesystem::exists(shader_path)) {
            LOG_ERROR("Shader file not found: {}", shader_path.string());
            return {};
        }
        
        ShaderCompiler compiler("../shaders", "../shaders_cache");
        auto result = compiler.compile(
            shader_path.string(),
            false  // no force recompile
        );
        
        if (!result.has_value()) {
            LOG_ERROR("Failed to compile shader: {}", shader_path.string());
            return {};
        }
        
        return result->spirv;
    }
    
    std::optional<Instance> instance_;
    std::optional<Device> device_;
    std::optional<Allocator> allocator_;
    std::optional<CommandPool> cmd_pool_;
    std::optional<Fence> fence_;
};

/**
 * @test GradientComputeTest.CompileShader
 * @brief Test shader compilation of gradient.slang (using Slang compiler uwu)
 * 
 * Verifies:
 * - Shader file exists
 * - Slang compilation succeeds
 * - SPIR-V is non-empty
 * - SPIR-V has valid structure
 */
TEST_F(GradientComputeTest, CompileShader) {
    auto spirv = compile_gradient_shader();
    
    ASSERT_FALSE(spirv.empty()) << "SPIR-V compilation failed";
    // SPIR-V is already in 32-bit words, so we just need to check it's non-empty
    ASSERT_GT(spirv.size(), 5) << "SPIR-V too small to be valid (needs header + instructions)";
    
    LOG_INFO("Compiled gradient.slang successfully with Slang: {} words ({} bytes)", 
             spirv.size(), spirv.size() * 4);
}

/**
 * @test GradientComputeTest.CreateStorageImage
 * @brief Test storage image creation for compute shader output
 * 
 * Verifies:
 * - Image creation with R8G8B8A8_UNORM format
 * - Image view creation
 * - Proper dimensions (1920x1080)
 * - STORAGE usage flag
 */
TEST_F(GradientComputeTest, CreateStorageImage) {
    constexpr u32 width = 1920;
    constexpr u32 height = 1080;
    constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    
    auto image_result = Image::create(
        *allocator_,
        width,
        height,
        format,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    );
    
    ASSERT_TRUE(image_result.has_value()) << "Failed to create storage image";
    
    auto& image = *image_result;
    EXPECT_EQ(image.format(), format);
    
    auto extent = image.extent();
    EXPECT_EQ(extent.width, width);
    EXPECT_EQ(extent.height, height);
    EXPECT_EQ(extent.depth, 1);
    
    EXPECT_NE(image.handle(), VK_NULL_HANDLE);
    EXPECT_NE(image.view(), VK_NULL_HANDLE);
    
    LOG_INFO("Created storage image: {}x{} {}", width, height, static_cast<int>(format));
}

/**
 * @test GradientComputeTest.CreatePipelineAndDescriptors
 * @brief Test compute pipeline and descriptor set creation
 * 
 * Verifies:
 * - Descriptor set layout creation (storage image binding)
 * - Compute pipeline creation with shader
 * - Descriptor pool creation
 * - Descriptor set allocation
 */
TEST_F(GradientComputeTest, CreatePipelineAndDescriptors) {
    // Compile shader
    auto spirv = compile_gradient_shader();
    ASSERT_FALSE(spirv.empty());
    
    // Create descriptor set layout
    auto layout_result = DescriptorSetLayoutBuilder()
        .add_binding(0, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(*device_);
    
    ASSERT_TRUE(layout_result.has_value()) << "Failed to create descriptor set layout";
    
    // Create compute pipeline
    auto pipeline_result = ComputePipelineBuilder()
        .with_shader(std::move(spirv))
        .with_descriptor_layout(layout_result->handle())
        .build(*device_);
    
    ASSERT_TRUE(pipeline_result.has_value()) << "Failed to create compute pipeline";
    
    // Create descriptor pool
    auto pool_result = DescriptorPool::create(*device_, 10);
    ASSERT_TRUE(pool_result.has_value()) << "Failed to create descriptor pool";
    
    // Allocate descriptor set
    auto descriptor_result = pool_result->allocate(*layout_result);
    ASSERT_TRUE(descriptor_result.has_value()) << "Failed to allocate descriptor set";
    
    LOG_INFO("Created pipeline and descriptors successfully");
}

/**
 * @test GradientComputeTest.DispatchGradientShader
 * @brief Full end-to-end test of gradient compute shader (Slang → SPIR-V → GPU)
 * 
 * This is the comprehensive test that exercises the entire compute pipeline:
 * 1. Compile gradient.slang shader with Slang compiler
 * 2. Create 1920x1080 storage image
 * 3. Create descriptor set layout and pipeline
 * 4. Allocate and bind descriptor set
 * 5. Record command buffer (image barriers, bind pipeline, dispatch)
 * 6. Submit to GPU and wait for completion
 * 
 * Verifies:
 * - Complete workflow executes without errors
 * - GPU dispatch completes successfully
 * - No validation errors (if enabled)
 */
TEST_F(GradientComputeTest, DispatchGradientShader) {
    // 1. Compile shader
    auto spirv = compile_gradient_shader();
    ASSERT_FALSE(spirv.empty()) << "Shader compilation failed";
    
    // 2. Create storage image (1920x1080)
    constexpr u32 width = 1920;
    constexpr u32 height = 1080;
    
    auto image_result = Image::create(
        *allocator_,
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    );
    ASSERT_TRUE(image_result.has_value()) << "Image creation failed";
    auto& image = *image_result;
    
    // 3. Create descriptor set layout
    auto layout_result = DescriptorSetLayoutBuilder()
        .add_binding(0, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(*device_);
    ASSERT_TRUE(layout_result.has_value()) << "Descriptor layout creation failed";
    auto& layout = *layout_result;
    
    // 4. Create compute pipeline
    auto pipeline_result = ComputePipelineBuilder()
        .with_shader(std::move(spirv))
        .with_entry_point("main")
        .with_descriptor_layout(layout.handle())
        .build(*device_);
    ASSERT_TRUE(pipeline_result.has_value()) << "Pipeline creation failed";
    auto& pipeline = *pipeline_result;
    
    // 5. Create descriptor pool and allocate set
    auto pool_result = DescriptorPool::create(*device_, 10);
    ASSERT_TRUE(pool_result.has_value()) << "Descriptor pool creation failed";
    auto& pool = *pool_result;
    
    auto descriptor_result = pool.allocate(layout);
    ASSERT_TRUE(descriptor_result.has_value()) << "Descriptor allocation failed";
    auto& descriptor_set = *descriptor_result;
    
    // 6. Bind storage image to descriptor set
    descriptor_set
        .bind_storage_image(0, image.view(), VK_IMAGE_LAYOUT_GENERAL)
        .update();
    
    // 7. Allocate command buffer
    auto cmd_result = CommandBuffer::allocate(*cmd_pool_);
    ASSERT_TRUE(cmd_result.has_value()) << "Command buffer allocation failed";
    auto& cmd = *cmd_result;
    
    // 8. Record command buffer
    cmd.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    
    // Transition image to GENERAL layout for compute shader access
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image.handle(),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    
    vkCmdPipelineBarrier(
        cmd.handle(),
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    // Bind pipeline and descriptor set
    pipeline.bind(cmd.handle());
    descriptor_set.bind(cmd.handle(), pipeline.layout(), 0);
    
    // Dispatch compute shader
    // Workgroup size: 8x8 (from shader)
    // Image size: 1920x1080
    // Workgroups: ceil(1920/8) x ceil(1080/8) = 240 x 135
    constexpr u32 workgroup_x = (width + 7) / 8;   // 240
    constexpr u32 workgroup_y = (height + 7) / 8;  // 135
    
    pipeline.dispatch(cmd.handle(), workgroup_x, workgroup_y, 1);
    
    cmd.end();
    
    // 9. Submit command buffer to compute queue
    auto queue_families = device_->queue_families();
    ASSERT_TRUE(queue_families.compute.has_value());
    
    VkQueue compute_queue = device_->compute_queue();
    ASSERT_NE(compute_queue, VK_NULL_HANDLE);
    
    VkCommandBuffer cmd_handle = cmd.handle();
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_handle,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    
    VkResult result = vkQueueSubmit(compute_queue, 1, &submit_info, fence_->handle());
    ASSERT_EQ(result, VK_SUCCESS) << "Queue submit failed";
    
    // 10. Wait for GPU to finish
    auto wait_result = fence_->wait(1'000'000'000);  // 1 second timeout
    ASSERT_TRUE(wait_result.has_value()) << "Fence wait failed or timed out";
    
    LOG_INFO("Gradient shader dispatched successfully! Generated {}x{} gradient image", width, height);
}

/**
 * @test GradientComputeTest.MultipleDispatches
 * @brief Test multiple dispatches to same image
 * 
 * Verifies:
 * - Pipeline can be reused for multiple dispatches
 * - Descriptor sets can be reused
 * - Command buffers can be reset and re-recorded
 */
TEST_F(GradientComputeTest, MultipleDispatches) {
    // Setup (same as DispatchGradientShader but condensed)
    auto spirv = compile_gradient_shader();
    ASSERT_FALSE(spirv.empty());
    
    auto image_result = Image::create(
        *allocator_, 256, 256, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT, VMA_MEMORY_USAGE_GPU_ONLY, 0
    );
    ASSERT_TRUE(image_result.has_value());
    
    auto layout_result = DescriptorSetLayoutBuilder()
        .add_binding(0, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(*device_);
    ASSERT_TRUE(layout_result.has_value());
    
    auto pipeline_result = ComputePipelineBuilder()
        .with_shader(std::move(spirv))
        .with_descriptor_layout(layout_result->handle())
        .build(*device_);
    ASSERT_TRUE(pipeline_result.has_value());
    
    auto pool_result = DescriptorPool::create(*device_, 10);
    ASSERT_TRUE(pool_result.has_value());
    
    auto descriptor_result = pool_result->allocate(*layout_result);
    ASSERT_TRUE(descriptor_result.has_value());
    
    descriptor_result->bind_storage_image(0, image_result->view(), VK_IMAGE_LAYOUT_GENERAL)
        .update();
    
    auto cmd_result = CommandBuffer::allocate(*cmd_pool_);
    ASSERT_TRUE(cmd_result.has_value());
    
    // Dispatch 3 times
    for (int i = 0; i < 3; ++i) {
        fence_->reset();
        
        cmd_result->reset();
        cmd_result->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        
        // Image barrier
        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image_result->handle(),
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        
        vkCmdPipelineBarrier(
            cmd_result->handle(),
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );
        
        pipeline_result->bind(cmd_result->handle());
        descriptor_result->bind(cmd_result->handle(), pipeline_result->layout(), 0);
        pipeline_result->dispatch(cmd_result->handle(), 32, 32, 1);
        
        cmd_result->end();
        
        VkCommandBuffer cmd_handle = cmd_result->handle();
        VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd_handle,
        };
        
        VkResult result = vkQueueSubmit(device_->compute_queue(), 1, &submit_info, fence_->handle());
        ASSERT_EQ(result, VK_SUCCESS);
        
        auto wait_result = fence_->wait(1'000'000'000);
        ASSERT_TRUE(wait_result.has_value());
    }
    
    LOG_INFO("Multiple dispatches completed successfully");
}
