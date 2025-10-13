/**
 * @file imgui_context.cpp
 * @brief ImGui integration implementation for LUMA Engine
 * 
 * This file implements the ImGuiContext wrapper class, handling initialization
 * of ImGui with GLFW and Vulkan backends, resource creation (descriptor pool,
 * render pass), and per-frame rendering logic.
 * 
 * Implementation notes:
 * - ImGui context is a global singleton (only one per application)
 * - GLFW backend installed with install_callbacks=true (handles all input)
 * - Vulkan backend uses dynamic rendering (no explicit render pass in backend)
 * - Descriptor pool sized for 1000 textures (1000 * sampled_image descriptors)
 * - Render pass has single subpass with color attachment (RGBA8_SRGB)
 * - Load op LOAD (preserve previous frame), store op STORE (keep ImGui output)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 */

#include <luma/editor/imgui_context.hpp>
#include <luma/core/logging.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace luma::editor {

/**
 * @brief Private constructor (use create() factory)
 * 
 * @param device Vulkan device
 * @param descriptor_pool ImGui descriptor pool
 * @param render_pass ImGui render pass
 */
ImGuiContext::ImGuiContext(
    const vulkan::Device& device,
    VkDescriptorPool descriptor_pool,
    VkRenderPass render_pass
) : device_(&device),
    descriptor_pool_(descriptor_pool),
    render_pass_(render_pass) {
}

/**
 * @brief Move constructor
 * 
 * @param other Source context (will be left in valid but unspecified state)
 */
ImGuiContext::ImGuiContext(ImGuiContext&& other) noexcept
    : device_(other.device_),
      descriptor_pool_(other.descriptor_pool_),
      render_pass_(other.render_pass_) {
    
    // leave other in valid state (nulled out)
    other.device_ = nullptr;
    other.descriptor_pool_ = VK_NULL_HANDLE;
    other.render_pass_ = VK_NULL_HANDLE;
}

/**
 * @brief Move assignment
 * 
 * @param other Source context
 * @return Reference to this
 */
auto ImGuiContext::operator=(ImGuiContext&& other) noexcept -> ImGuiContext& {
    if (this != &other) {
        // cleanup current resources
        if (device_ != nullptr) {
            if (descriptor_pool_ != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device_->handle(), descriptor_pool_, nullptr);
            }
            if (render_pass_ != VK_NULL_HANDLE) {
                vkDestroyRenderPass(device_->handle(), render_pass_, nullptr);
            }
        }
        
        // move from other
        device_ = other.device_;
        descriptor_pool_ = other.descriptor_pool_;
        render_pass_ = other.render_pass_;
        
        // leave other in valid state
        other.device_ = nullptr;
        other.descriptor_pool_ = VK_NULL_HANDLE;
        other.render_pass_ = VK_NULL_HANDLE;
    }
    return *this;
}

/**
 * @brief Destructor (cleanup ImGui resources)
 */
ImGuiContext::~ImGuiContext() {
    if (device_ == nullptr) {
        return; // moved-from object
    }
    
    // wait for device to be idle before destroying resources
    vkDeviceWaitIdle(device_->handle());
    
    // shutdown ImGui backends (must be done before destroying Vulkan resources)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // destroy Vulkan resources
    if (descriptor_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_->handle(), descriptor_pool_, nullptr);
    }
    
    if (render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_->handle(), render_pass_, nullptr);
    }
    
    LOG_INFO("ImGui context destroyed");
}

/**
 * @brief Creates ImGui context with GLFW and Vulkan backends
 * 
 * @param instance Vulkan instance
 * @param device Vulkan device
 * @param window GLFW window
 * @param swapchain Swapchain
 * @return ImGuiContext on success, Error on failure
 */
auto ImGuiContext::create(
    const vulkan::Instance& instance,
    const vulkan::Device& device,
    const Window& window,
    const vulkan::Swapchain& swapchain
) -> Result<ImGuiContext> {
    
    LOG_INFO("Creating ImGui context...");
    
    // create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // enable docking and viewports
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // when viewports are enabled, tweak WindowRounding so platform windows can look identical
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f; // opaque background
    }
    
    // setup ImGui style (dark theme)
    ImGui::StyleColorsDark();
    
    // init GLFW backend
    if (!ImGui_ImplGlfw_InitForVulkan(window.handle(), true)) {
        ImGui::DestroyContext();
        return std::unexpected(Error{
            ErrorCode::CORE_INITIALIZATION_FAILED,
            "Failed to initialize ImGui GLFW backend"
        });
    }
    
    // create descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<u32>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    if (vkCreateDescriptorPool(device.handle(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            "Failed to create ImGui descriptor pool"
        });
    }
    
    // create render pass for ImGui
    VkAttachmentDescription attachment{};
    attachment.format = swapchain.format();
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // preserve previous content
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference color_attachment{};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;
    
    VkRenderPass render_pass{VK_NULL_HANDLE};
    if (vkCreateRenderPass(device.handle(), &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
        vkDestroyDescriptorPool(device.handle(), descriptor_pool, nullptr);
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            "Failed to create ImGui render pass"
        });
    }
    
    // init Vulkan backend
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = instance.handle();
    init_info.PhysicalDevice = device.physical_device();
    init_info.Device = device.handle();
    init_info.QueueFamily = device.queue_families().graphics.value();
    init_info.Queue = device.graphics_queue();
    init_info.DescriptorPool = descriptor_pool;
    init_info.MinImageCount = static_cast<u32>(swapchain.images().size());
    init_info.ImageCount = static_cast<u32>(swapchain.images().size());
    init_info.CheckVkResultFn = [](VkResult result) {
        if (result != VK_SUCCESS) {
            LOG_ERROR("ImGui Vulkan error: {}", static_cast<int>(result));
        }
    };
    
    // Initialize with render pass (backend will create pipeline automatically)
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        vkDestroyRenderPass(device.handle(), render_pass, nullptr);
        vkDestroyDescriptorPool(device.handle(), descriptor_pool, nullptr);
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return std::unexpected(Error{
            ErrorCode::CORE_INITIALIZATION_FAILED,
            "Failed to initialize ImGui Vulkan backend"
        });
    }
    
    // Create main pipeline for rendering
    ImGui_ImplVulkan_PipelineInfo pipeline_info{};
    pipeline_info.RenderPass = render_pass;
    pipeline_info.Subpass = 0;
    pipeline_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_CreateMainPipeline(&pipeline_info);
    
    LOG_INFO("ImGui context created successfully");
    LOG_INFO("ImGui version: {}", IMGUI_VERSION);
    LOG_INFO("ImGui docking: {}", (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) ? "enabled" : "disabled");
    LOG_INFO("ImGui viewports: {}", (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) ? "enabled" : "disabled");
    
    return ImGuiContext{device, descriptor_pool, render_pass};
}

/**
 * @brief Begins new ImGui frame
 */
auto ImGuiContext::begin_frame() -> void {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

/**
 * @brief Ends ImGui frame
 */
auto ImGuiContext::end_frame() -> void {
    ImGui::Render();
    
    // update and render additional platform windows (if viewports enabled)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

/**
 * @brief Renders ImGui to command buffer
 * 
 * @param command_buffer Vulkan command buffer
 * @param framebuffer Vulkan framebuffer
 * @param width Framebuffer width
 * @param height Framebuffer height
 */
auto ImGuiContext::render(VkCommandBuffer command_buffer, VkFramebuffer framebuffer,
                          u32 width, u32 height) -> void {
    
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->TotalVtxCount == 0) {
        return; // nothing to render
    }
    
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {width, height};
    
    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    
    // record ImGui draw data to command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
    
    vkCmdEndRenderPass(command_buffer);
}

} // namespace luma::editor
