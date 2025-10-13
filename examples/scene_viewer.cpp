/**
 * @file scene_viewer.cpp
 * @brief Interactive scene viewer with SDF rendering and ImGui UI
 * 
 * This example demonstrates:
 * 1. Loading YAML scenes (Pong and test scenes)
 * 2. Uploading geometry to GPU (SDF primitives)
 * 3. Ray marching compute shader rendering
 * 4. ImGui scene switching and hierarchy display
 * 5. Real-time window with swapchain presentation
 * 
 * ✨ COMPLETE PHASE 1 INTEGRATION TEST ✨
 * 
 * @author LukeFrankio
 * @date 2025-10-13
 * 
 * @note Requires Vulkan 1.3+ and compute queue support
 * @note Requires Slang compiler (slangc.exe)
 * @note Default resolution: 1280x720 (adjustable)
 */

#include <luma/asset/shader_compiler.hpp>
#include <luma/core/logging.hpp>
#include <luma/input/window.hpp>
#include <luma/scene/serialization.hpp>
#include <luma/scene/world.hpp>
#include <luma/vulkan/command_buffer.hpp>
#include <luma/vulkan/descriptor.hpp>
#include <luma/vulkan/device.hpp>
#include <luma/vulkan/instance.hpp>
#include <luma/vulkan/memory.hpp>
#include <luma/vulkan/pipeline.hpp>
#include <luma/vulkan/swapchain.hpp>
#include <luma/vulkan/sync.hpp>

// Disable warnings for ImGui (third-party library with old-style casts)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#include <luma/editor/imgui_context.hpp>
#include <imgui.h>
#pragma GCC diagnostic pop

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace luma;
using namespace luma::vulkan;
using namespace luma::asset;
using namespace luma::scene;
using namespace luma::editor;

//============================================================================
// GPU Data Structures (must match shader layout!)
//============================================================================

/**
 * @brief Transform component GPU layout
 * 
 * Matches Slang struct in sdf_renderer.slang
 */
struct TransformGPU {
    alignas(16) vec3 position;
    f32 _pad0;
    alignas(16) quat rotation;  // Quaternion (w, x, y, z)
    alignas(16) vec3 scale;
    f32 _pad1;
};

/**
 * @brief Geometry component GPU layout
 */
struct GeometryGPU {
    u32 type;        // SDFType enum (0=Sphere, 1=Box, 2=Plane)
    vec3 params;     // Type-specific parameters
    f32 rounding;    // Rounding radius
    vec3 _pad;
};

/**
 * @brief Material component GPU layout
 */
struct MaterialGPU {
    vec3 base_color;
    f32 metallic;
    f32 roughness;
    vec3 emissive_color;
};

/**
 * @brief Combined entity data for GPU
 */
struct EntityDataGPU {
    TransformGPU transform;
    GeometryGPU geometry;
    MaterialGPU material;
};

/**
 * @brief Camera uniform data
 */
struct CameraDataGPU {
    alignas(16) vec3 position;
    f32 _pad0;
    alignas(16) vec3 forward;
    f32 _pad1;
    alignas(16) vec3 up;
    f32 _pad2;
    alignas(16) vec3 right;
    f32 _pad3;
    alignas(8) vec2 view_size;
    f32 near_plane;
    f32 far_plane;
};

/**
 * @brief Push constants
 */
struct PushConstants {
    u32 entity_count;
    u32 _pad0;
    u32 _pad1;
    u32 _pad2;
};

//============================================================================
// Helper Functions
//============================================================================

/**
 * @brief Convert ECS world to GPU entity data
 * 
 * ✨ PURE FUNCTION ✨ (reads world, returns vector)
 */
auto extract_entity_data(const World& world) -> std::vector<EntityDataGPU> {
    std::vector<EntityDataGPU> entities;
    
    // Query all entities with Transform + Geometry + Material
    world.each<Transform, Geometry, Material>([&]([[maybe_unused]] Entity e, 
                                                    const Transform& t, 
                                                    const Geometry& g, 
                                                    const Material& m) {
        EntityDataGPU gpu_data{};
        
        // Copy transform
        gpu_data.transform.position = t.position;
        gpu_data.transform.rotation = t.rotation;
        gpu_data.transform.scale = t.scale;
        
        // Copy geometry
        gpu_data.geometry.type = static_cast<u32>(g.type);
        
        // Convert geometry params based on type
        if (g.type == SDFType::SPHERE) {
            gpu_data.geometry.params = vec3(g.params.x, 0.0f, 0.0f);  // radius
        } else if (g.type == SDFType::BOX) {
            gpu_data.geometry.params = vec3(g.params.x, g.params.y, g.params.z);  // extents
        } else if (g.type == SDFType::PLANE) {
            gpu_data.geometry.params = vec3(g.params.x, g.params.y, g.params.z);  // normal
        }
        
        gpu_data.geometry.rounding = g.rounding;
        
        // Copy material
        gpu_data.material.base_color = m.base_color;
        gpu_data.material.metallic = m.metallic;
        gpu_data.material.roughness = m.roughness;
        gpu_data.material.emissive_color = m.emissive_color;
        
        entities.push_back(gpu_data);
    });
    
    return entities;
}

/**
 * @brief Create orthographic camera looking down Z axis
 * 
 * ✨ PURE FUNCTION ✨
 */
auto create_orthographic_camera(f32 view_width, f32 view_height) -> CameraDataGPU {
    CameraDataGPU camera{};
    
    // Position camera BEHIND scene looking TOWARD it (camera at -Z looking toward +Z)
    camera.position = vec3(0.0f, 0.0f, -30.0f);
    camera.forward = vec3(0.0f, 0.0f, 1.0f);  // Looking toward +Z (toward scene at z=0)
    camera.up = vec3(0.0f, 1.0f, 0.0f);
    camera.right = vec3(1.0f, 0.0f, 0.0f);
    
    // View plane size (world units visible)
    camera.view_size = vec2(view_width, view_height);
    camera.near_plane = 0.1f;
    camera.far_plane = 100.0f;
    
    return camera;
}

/**
 * @brief Render ImGui scene hierarchy
 * 
 * ⚠️ IMPURE (ImGui state)
 */
auto render_scene_hierarchy(const World& world) -> void {
    ImGui::Begin("Scene Hierarchy");
    
    ImGui::Text("Entities:");
    ImGui::Separator();
    
    // Display all entities with their components
    world.each<Transform>([&](Entity e, const Transform& t) {
        // Get entity name if it has one
        std::string name = "Entity " + std::to_string(e.id());
        auto name_component = world.get_component<Name>(e);
        if (name_component) {
            name = name_component->value;
        }
        
        if (ImGui::TreeNode(name.c_str())) {
            // Transform info
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                       static_cast<double>(t.position.x), 
                       static_cast<double>(t.position.y), 
                       static_cast<double>(t.position.z));
            ImGui::Text("Scale: (%.2f, %.2f, %.2f)", 
                       static_cast<double>(t.scale.x), 
                       static_cast<double>(t.scale.y), 
                       static_cast<double>(t.scale.z));
            
            // Geometry info
            auto geometry = world.get_component<Geometry>(e);
            if (geometry) {
                const char* type_name = "Unknown";
                if (geometry->type == SDFType::SPHERE) type_name = "Sphere";
                else if (geometry->type == SDFType::BOX) type_name = "Box";
                else if (geometry->type == SDFType::PLANE) type_name = "Plane";
                
                ImGui::Text("Geometry: %s", type_name);
                ImGui::Text("Params: (%.2f, %.2f, %.2f)", 
                           static_cast<double>(geometry->params.x), 
                           static_cast<double>(geometry->params.y), 
                           static_cast<double>(geometry->params.z));
                if (geometry->rounding > 0.0f) {
                    ImGui::Text("Rounding: %.2f", static_cast<double>(geometry->rounding));
                }
            }
            
            // Material info
            auto material = world.get_component<Material>(e);
            if (material) {
                auto base_color = material->base_color;  // Make mutable copy
                ImGui::ColorEdit3("Base Color", &base_color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
                ImGui::Text("Metallic: %.2f", static_cast<double>(material->metallic));
                ImGui::Text("Roughness: %.2f", static_cast<double>(material->roughness));
                if (glm::length(material->emissive_color) > 0.01f) {
                    auto emissive = material->emissive_color;  // Make mutable copy
                    ImGui::ColorEdit3("Emissive", &emissive.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker);
                }
            }
            
            // Velocity info
            auto velocity = world.get_component<Velocity>(e);
            if (velocity) {
                ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", 
                           static_cast<double>(velocity->linear.x), 
                           static_cast<double>(velocity->linear.y), 
                           static_cast<double>(velocity->linear.z));
            }
            
            ImGui::TreePop();
        }
    });
    
    ImGui::End();
}

//============================================================================
// Main Application
//============================================================================

auto main() -> int {
    LOG_INFO("=== Scene Viewer with SDF Rendering ===");
    LOG_INFO("Phase 1 Integration Test - Scene Loading + GPU Rendering");
    
    // Window dimensions
    constexpr u32 WIDTH = 1280;
    constexpr u32 HEIGHT = 720;
    
    // Step 1: Create window
    LOG_INFO("Creating window...");
    auto window_result = Window::create("LUMA Scene Viewer", WIDTH, HEIGHT);
    if (!window_result) {
        LOG_ERROR("Failed to create window");
        return EXIT_FAILURE;
    }
    auto window = std::move(*window_result);
    LOG_INFO("✓ Window created ({}x{})", WIDTH, HEIGHT);
    
    // Step 2: Create Vulkan instance
    auto instance_result = Instance::create("SceneViewer", VK_MAKE_API_VERSION(0, 1, 0, 0));
    if (!instance_result) {
        LOG_ERROR("Failed to create Vulkan instance");
        return EXIT_FAILURE;
    }
    auto instance = std::move(*instance_result);
    LOG_INFO("✓ Vulkan instance created");
    
    // Step 3: Create surface
    auto surface_result = window.create_surface(instance.handle());
    if (!surface_result) {
        LOG_ERROR("Failed to create surface");
        return EXIT_FAILURE;
    }
    VkSurfaceKHR surface = *surface_result;
    LOG_INFO("✓ Surface created");
    
    // Step 4: Create device with present support
    auto device_result = Device::create(instance, surface, {VK_KHR_SWAPCHAIN_EXTENSION_NAME});
    if (!device_result) {
        LOG_ERROR("Failed to create device");
        return EXIT_FAILURE;
    }
    auto device = std::move(*device_result);
    LOG_INFO("✓ Device created");
    
    // Step 5: Create swapchain
    auto swapchain_result = Swapchain::create(device, surface, WIDTH, HEIGHT);
    if (!swapchain_result) {
        LOG_ERROR("Failed to create swapchain");
        return EXIT_FAILURE;
    }
    auto swapchain = std::move(*swapchain_result);
    LOG_INFO("✓ Swapchain created");
    
    // Step 6: Create memory allocator
    auto allocator_result = Allocator::create(instance, device);
    if (!allocator_result) {
        LOG_ERROR("Failed to create allocator");
        return EXIT_FAILURE;
    }
    auto allocator = std::move(*allocator_result);
    LOG_INFO("✓ Memory allocator created");
    
    // Step 7: Create command pool
    auto queue_families = device.queue_families();
    auto cmd_pool_result = CommandPool::create(
        device, 
        *queue_families.graphics,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );
    if (!cmd_pool_result) {
        LOG_ERROR("Failed to create command pool");
        return EXIT_FAILURE;
    }
    auto cmd_pool = std::move(*cmd_pool_result);
    LOG_INFO("✓ Command pool created");
    
    // Step 8: Initialize ImGui
    LOG_INFO("Initializing ImGui...");
    auto imgui_result = luma::editor::ImGuiContext::create(instance, device, window, swapchain);
    if (!imgui_result) {
        LOG_ERROR("Failed to initialize ImGui");
        return EXIT_FAILURE;
    }
    auto imgui_ctx = std::move(*imgui_result);
    LOG_INFO("✓ ImGui initialized");
    
    // Step 8b: Create framebuffers for swapchain images
    std::vector<VkFramebuffer> framebuffers;
    framebuffers.resize(swapchain.image_views().size());
    for (size_t i = 0; i < swapchain.image_views().size(); i++) {
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = imgui_ctx.render_pass();
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &swapchain.image_views()[i];
        framebuffer_info.width = swapchain.extent().width;
        framebuffer_info.height = swapchain.extent().height;
        framebuffer_info.layers = 1;
        
        if (vkCreateFramebuffer(device.handle(), &framebuffer_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create framebuffer {}", i);
            // Cleanup already created framebuffers
            for (size_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(device.handle(), framebuffers[j], nullptr);
            }
            return EXIT_FAILURE;
        }
    }
    LOG_INFO("✓ Created {} framebuffers", framebuffers.size());
    
    // Step 9: Create sync primitives (3 frames in flight)
    constexpr u32 MAX_FRAMES_IN_FLIGHT = 3;
    std::vector<Fence> fences;
    std::vector<Semaphore> image_available_semaphores;
    std::vector<Semaphore> render_finished_semaphores;
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto fence_result = Fence::create(device.handle(), true);  // Signaled
        if (!fence_result) {
            LOG_ERROR("Failed to create fence");
            return EXIT_FAILURE;
        }
        fences.push_back(std::move(*fence_result));
        
        auto sem1_result = Semaphore::create(device.handle());
        auto sem2_result = Semaphore::create(device.handle());
        if (!sem1_result || !sem2_result) {
            LOG_ERROR("Failed to create semaphores");
            return EXIT_FAILURE;
        }
        image_available_semaphores.push_back(std::move(*sem1_result));
        render_finished_semaphores.push_back(std::move(*sem2_result));
    }
    LOG_INFO("✓ Synchronization primitives created ({} frames in flight)", MAX_FRAMES_IN_FLIGHT);
    
    // Step 10: Load scenes
    LOG_INFO("Loading scenes...");
    
    World pong_world;
    World test_world;
    
    auto pong_result = load_scene(pong_world, "assets/scenes/pong_scene.yaml");
    if (!pong_result) {
        LOG_ERROR("Failed to load pong_scene.yaml");
        return EXIT_FAILURE;
    }
    LOG_INFO("✓ Loaded pong_scene.yaml ({} entities)", pong_world.entity_count());
    
    auto test_result = load_scene(test_world, "assets/scenes/test_scene.yaml");
    if (!test_result) {
        LOG_ERROR("Failed to load test_scene.yaml");
        return EXIT_FAILURE;
    }
    LOG_INFO("✓ Loaded test_scene.yaml ({} entities)", test_world.entity_count());
    
    // Current scene (start with Pong)
    World* current_world = &pong_world;
    int current_scene_index = 0;  // 0=Pong, 1=Test
    const char* scene_names[] = {"Pong Scene", "Test Scene"};
    
    // Step 11: Compile SDF renderer shader
    LOG_INFO("Compiling sdf_renderer.slang shader...");
    ShaderCompiler compiler("shaders", "shaders_cache");
    auto shader_result = compiler.compile("sdf_renderer.slang", false);
    if (!shader_result) {
        LOG_ERROR("Failed to compile sdf_renderer shader");
        return EXIT_FAILURE;
    }
    const auto& shader_module = *shader_result;
    LOG_INFO("✓ Shader compiled: {} SPIR-V words", shader_module.spirv.size());
    
    // Step 12: Create render image (compute shader output)
    auto render_image_result = Image::create(
        allocator,
        WIDTH,
        HEIGHT,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        0
    );
    if (!render_image_result) {
        LOG_ERROR("Failed to create render image");
        return EXIT_FAILURE;
    }
    auto render_image = std::move(*render_image_result);
    LOG_INFO("✓ Render image created");
    
    // Step 13: Create descriptor set layout
    auto descriptor_layout_result = DescriptorSetLayoutBuilder()
        .add_binding(0, DescriptorType::storage_image, VK_SHADER_STAGE_COMPUTE_BIT)
        .add_binding(1, DescriptorType::uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT)
        .add_binding(2, DescriptorType::storage_buffer, VK_SHADER_STAGE_COMPUTE_BIT)
        .build(device);
    if (!descriptor_layout_result) {
        LOG_ERROR("Failed to create descriptor set layout");
        return EXIT_FAILURE;
    }
    auto descriptor_layout = std::move(*descriptor_layout_result);
    LOG_INFO("✓ Descriptor set layout created");
    
    // Step 14: Create descriptor pool
    auto descriptor_pool_result = DescriptorPool::create(device, 10);
    if (!descriptor_pool_result) {
        LOG_ERROR("Failed to create descriptor pool");
        return EXIT_FAILURE;
    }
    auto descriptor_pool = std::move(*descriptor_pool_result);
    LOG_INFO("✓ Descriptor pool created");
    
    // Step 15: Create camera buffer
    auto camera_buffer_result = Buffer::create(
        allocator,
        sizeof(CameraDataGPU),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    if (!camera_buffer_result) {
        LOG_ERROR("Failed to create camera buffer");
        return EXIT_FAILURE;
    }
    auto camera_buffer = std::move(*camera_buffer_result);
    LOG_INFO("✓ Camera buffer created");
    
    // Initialize camera
    const CameraDataGPU camera_data = create_orthographic_camera(40.0f, 22.5f);  // Pong view size
    auto camera_upload = camera_buffer.map_and_write(std::span(&camera_data, 1));
    if (!camera_upload) {
        LOG_ERROR("Failed to upload camera data: {}", camera_upload.error().message);
        return EXIT_FAILURE;
    }
    
    // Step 16: Create entity buffer (dynamic - will be recreated per scene)
    std::unique_ptr<Buffer> entity_buffer;
    
    // Step 17: Create compute pipeline with push constants
    PushConstantRange push_constant{};
    push_constant.stage_flags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(PushConstants);
    
    auto pipeline_result = ComputePipelineBuilder()
        .with_shader(shader_module.spirv)
        .with_descriptor_layout(descriptor_layout.handle())
        .with_push_constants(push_constant)
        .build(device);
    if (!pipeline_result) {
        LOG_ERROR("Failed to create compute pipeline");
        return EXIT_FAILURE;
    }
    auto pipeline = std::move(*pipeline_result);
    LOG_INFO("✓ Compute pipeline created");
    
    // Step 18: Allocate command buffers
    std::vector<CommandBuffer> command_buffers;
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto cmd_result = CommandBuffer::allocate(cmd_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        if (!cmd_result) {
            LOG_ERROR("Failed to allocate command buffer");
            return EXIT_FAILURE;
        }
        command_buffers.push_back(std::move(*cmd_result));
    }
    LOG_INFO("✓ Command buffers allocated");
    
    // Helper function to upload scene to GPU
    auto upload_scene_to_gpu = [&](const World& world) -> Result<void> {
        // Extract entity data
        const auto entity_data = extract_entity_data(world);
        
        if (entity_data.empty()) {
            LOG_WARN("Scene has no renderable entities");
            return {};
        }
        
        LOG_INFO("Uploading {} entities to GPU...", entity_data.size());
        
        // Create/recreate entity buffer
        const VkDeviceSize buffer_size = entity_data.size() * sizeof(EntityDataGPU);
        auto buffer_result = Buffer::create(
            allocator,
            buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        
        if (!buffer_result) {
            return std::unexpected(Error{
                ErrorCode::VULKAN_OPERATION_FAILED,
                "Failed to create entity buffer"
            });
        }
        
        entity_buffer = std::make_unique<Buffer>(std::move(*buffer_result));
        auto upload_result = entity_buffer->map_and_write(std::span(entity_data));
        if (!upload_result) {
            return std::unexpected(upload_result.error());
        }
        
        LOG_INFO("✓ Entities uploaded to GPU");
        return {};
    };
    
    // Upload initial scene
    auto upload_result = upload_scene_to_gpu(*current_world);
    if (!upload_result) {
        LOG_ERROR("Failed to upload initial scene");
        return EXIT_FAILURE;
    }
    
    // Step 19: Create descriptor set
    auto descriptor_set_result = descriptor_pool.allocate(descriptor_layout);
    if (!descriptor_set_result) {
        LOG_ERROR("Failed to allocate descriptor set");
        return EXIT_FAILURE;
    }
    auto descriptor_set = std::move(*descriptor_set_result);
    
    // Validate entity buffer exists
    if (!entity_buffer) {
        LOG_ERROR("Entity buffer is null - scene upload failed!");
        return EXIT_FAILURE;
    }
    
    descriptor_set.bind_storage_image(0, render_image.view(), VK_IMAGE_LAYOUT_GENERAL);
    descriptor_set.bind_uniform_buffer(1, camera_buffer.handle(), 0, sizeof(CameraDataGPU));
    descriptor_set.bind_storage_buffer(2, entity_buffer->handle(), 0, VK_WHOLE_SIZE);
    descriptor_set.update();
    LOG_INFO("✓ Descriptor set bound (image, camera, {} entity bytes)", 
            entity_buffer->size());
    
    // Main loop
    LOG_INFO("=== Entering Main Loop ===");
    LOG_INFO("Camera: pos=(0,0,-30) forward=(0,0,1) view_size=(40x22.5)");
    LOG_INFO("Press ESC or close window to exit");
    u32 frame_index = 0;
    bool scene_changed = false;
    
    while (!window.should_close()) {
        window.poll_events();
        
        // Wait for frame to be available
        const u32 current_frame = frame_index % MAX_FRAMES_IN_FLIGHT;
        fences[current_frame].wait(UINT64_MAX);
        
        // Acquire swapchain image
        auto acquire_result = swapchain.acquire_next_image(
            image_available_semaphores[current_frame].handle()
        );
        
        if (!acquire_result) {
            LOG_ERROR("Failed to acquire swapchain image");
            break;
        }
        u32 image_index = *acquire_result;
        
        // Reset fence
        fences[current_frame].reset();
        
        // Start ImGui frame
        imgui_ctx.begin_frame();
        
        // Scene selector UI
        ImGui::Begin("Scene Selector");
        ImGui::Text("Active Scene:");
        if (ImGui::Combo("##scene", &current_scene_index, scene_names, 2)) {
            // Scene changed
            current_world = (current_scene_index == 0) ? &pong_world : &test_world;
            scene_changed = true;
            LOG_INFO("Switched to scene: {}", scene_names[current_scene_index]);
        }
        ImGui::Separator();
        ImGui::Text("FPS: %.1f", static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::Text("Frame Time: %.3f ms", static_cast<double>(1000.0f / ImGui::GetIO().Framerate));
        ImGui::End();
        
        // Scene hierarchy UI
        render_scene_hierarchy(*current_world);
        
        // If scene changed, re-upload to GPU
        if (scene_changed) {
            device.wait_idle();  // Wait for GPU to finish using old buffers
            
            auto upload = upload_scene_to_gpu(*current_world);
            if (upload) {
                // Rebind entity buffer in descriptor set
                descriptor_set.bind_storage_buffer(2, entity_buffer->handle(), 0, VK_WHOLE_SIZE);
                descriptor_set.update();
            }
            
            scene_changed = false;
        }
        
        imgui_ctx.end_frame();
        
        // Record command buffer
        auto& cmd = command_buffers[current_frame];
        cmd.reset();
        cmd.begin(0);
        
        // Transition render image to GENERAL layout
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = render_image.handle();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        
        vkCmdPipelineBarrier(
            cmd.handle(),
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );
        
        // Clear render image to known state (dark blue background)
        VkClearColorValue clear_color = {{0.05f, 0.05f, 0.15f, 1.0f}};
        VkImageSubresourceRange clear_range{};
        clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clear_range.baseMipLevel = 0;
        clear_range.levelCount = 1;
        clear_range.baseArrayLayer = 0;
        clear_range.layerCount = 1;
        vkCmdClearColorImage(
            cmd.handle(),
            render_image.handle(),
            VK_IMAGE_LAYOUT_GENERAL,
            &clear_color,
            1, &clear_range
        );
        
        // Dispatch compute shader
        pipeline.bind(cmd.handle());
        descriptor_set.bind(cmd.handle(), pipeline.layout(), 0);
        
        // Push constants (entity count)
        auto entity_data = extract_entity_data(*current_world);
        PushConstants push{};
        push.entity_count = static_cast<u32>(entity_data.size());
        
        // Validation check (first frame only)
        if (frame_index == 0) {
            LOG_INFO("First frame dispatch: {} entities, image {}x{}", 
                    push.entity_count, WIDTH, HEIGHT);
        }
        
        vkCmdPushConstants(
            cmd.handle(),
            pipeline.layout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(PushConstants),
            &push
        );
        
        constexpr u32 WORKGROUP_SIZE = 8;
        const u32 dispatch_x = (WIDTH + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        const u32 dispatch_y = (HEIGHT + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
        pipeline.dispatch(cmd.handle(), dispatch_x, dispatch_y, 1);
        
        // Transition render image for transfer
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(
            cmd.handle(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier
        );
        
        // Transition swapchain image for transfer
        VkImageMemoryBarrier swapchain_barrier{};
        swapchain_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapchain_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchain_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchain_barrier.image = swapchain.images()[image_index];
        swapchain_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapchain_barrier.subresourceRange.baseMipLevel = 0;
        swapchain_barrier.subresourceRange.levelCount = 1;
        swapchain_barrier.subresourceRange.baseArrayLayer = 0;
        swapchain_barrier.subresourceRange.layerCount = 1;
        swapchain_barrier.srcAccessMask = 0;
        swapchain_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(
            cmd.handle(),
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &swapchain_barrier
        );
        
        // Blit render image to swapchain
        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.srcOffsets[1] = {static_cast<i32>(WIDTH), static_cast<i32>(HEIGHT), 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.layerCount = 1;
        blit.dstOffsets[1] = {static_cast<i32>(swapchain.extent().width), 
                              static_cast<i32>(swapchain.extent().height), 1};
        
        vkCmdBlitImage(
            cmd.handle(),
            render_image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.images()[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );
        
        // Transition swapchain for presentation
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchain_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapchain_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        vkCmdPipelineBarrier(
            cmd.handle(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &swapchain_barrier
        );
        
        // Render ImGui
        imgui_ctx.render(cmd.handle(), framebuffers[image_index], 
                        swapchain.extent().width, swapchain.extent().height);
        
        // Transition swapchain for present
        swapchain_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchain_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchain_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        swapchain_barrier.dstAccessMask = 0;
        
        vkCmdPipelineBarrier(
            cmd.handle(),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &swapchain_barrier
        );
        
        cmd.end();
        
        // Submit command buffer
        VkCommandBuffer cmd_handle = cmd.handle();
        VkSemaphore wait_semaphore = image_available_semaphores[current_frame].handle();
        VkSemaphore signal_semaphore = render_finished_semaphores[current_frame].handle();
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &wait_semaphore;
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &cmd_handle;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &signal_semaphore;
        
        vkQueueSubmit(device.graphics_queue(), 1, &submit_info, fences[current_frame].handle());
        
        // Present
        auto present_result = swapchain.present(
            device.graphics_queue(),
            image_index,
            signal_semaphore
        );
        
        if (!present_result) {
            LOG_ERROR("Failed to present");
            break;
        }
        
        frame_index++;
    }
    
    // Wait for GPU to finish
    device.wait_idle();
    
    // Cleanup framebuffers
    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(device.handle(), framebuffer, nullptr);
    }
    
    LOG_INFO("=== Scene Viewer Terminated ===");
    LOG_INFO("✨ Phase 1 Integration Test Complete ✨");
    
    return EXIT_SUCCESS;
}
