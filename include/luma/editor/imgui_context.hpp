/**
 * @file imgui_context.hpp
 * @brief ImGui integration wrapper for LUMA Engine
 * 
 * This file provides RAII wrapper for ImGui initialization with GLFW and Vulkan
 * backends. ImGui is an immediate-mode GUI library used for debug overlays,
 * settings UI, profiler visualization, and scene hierarchy display.
 * 
 * Design decisions:
 * - Use docking branch of ImGui (split windows, tabs, docking spaces)
 * - GLFW backend handles input (keyboard, mouse, gamepad)
 * - Vulkan backend renders ImGui draw data to swapchain
 * - Descriptor pool sized for typical UI (1000 textures should be plenty)
 * - Single render pass for ImGui (no complex multipass rendering needed)
 * 
 * Integration approach:
 * 1. Init: Create ImGui context, init GLFW + Vulkan backends, create resources
 * 2. Per-frame:
 *    a. begin_frame(): Start new ImGui frame (NewFrame for backends + ImGui)
 *    b. (User code): Build UI with ImGui::* calls
 *    c. end_frame(): Render ImGui (Render + UpdatePlatformWindows)
 *    d. render(): Record Vulkan commands to draw ImGui to command buffer
 * 3. Shutdown: Cleanup ImGui resources (descriptor pool, render pass, etc.)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Uses C++26 features (std::expected, concepts)
 * @note Compiled with GCC 15+ (latest compiler)
 * @note Requires ImGui docking branch (latest version)
 * @note Requires GLFW 3.4+ and Vulkan 1.3+
 * 
 * example usage:
 * @code
 * // initialization
 * auto imgui_ctx = ImGuiContext::create(instance, device, window, swapchain);
 * 
 * // per frame
 * imgui_ctx.begin_frame();
 * 
 * ImGui::Begin("Settings");
 * ImGui::Text("FPS: %.1f", fps);
 * ImGui::SliderInt("SPP", &spp, 1, 8);
 * ImGui::End();
 * 
 * imgui_ctx.end_frame();
 * 
 * // in command buffer recording
 * imgui_ctx.render(command_buffer);
 * @endcode
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/vulkan/device.hpp>
#include <luma/vulkan/swapchain.hpp>
#include <luma/input/window.hpp>

#include <vulkan/vulkan.h>
#include <imgui.h>

namespace luma::editor {

/**
 * @class ImGuiContext
 * @brief RAII wrapper for ImGui with GLFW and Vulkan backends
 * 
 * This class manages the lifetime of ImGui context, backends, and Vulkan
 * resources needed for rendering. It follows RAII semantics (movable but
 * not copyable) and provides clean initialization/shutdown.
 * 
 * ImGui resources managed:
 * - ImGui context (global state)
 * - GLFW backend (input handling)
 * - Vulkan backend (rendering)
 * - Descriptor pool (for ImGui textures)
 * - Render pass (for ImGui rendering)
 * 
 * @note This class is movable but not copyable (unique ownership)
 * @note All methods are const-correct (begin_frame/end_frame/render are non-const)
 * @warning Must call begin_frame before ImGui:: calls each frame
 * @warning Must call end_frame after ImGui:: calls each frame
 * @warning Must call render in command buffer to actually draw ImGui
 */
class ImGuiContext {
public:
    /// @brief Deleted copy constructor (unique ownership)
    ImGuiContext(const ImGuiContext&) = delete;
    
    /// @brief Deleted copy assignment (unique ownership)
    auto operator=(const ImGuiContext&) -> ImGuiContext& = delete;
    
    /// @brief Move constructor (transfer ownership)
    ImGuiContext(ImGuiContext&& other) noexcept;
    
    /// @brief Move assignment (transfer ownership)
    auto operator=(ImGuiContext&& other) noexcept -> ImGuiContext&;
    
    /// @brief Destructor (cleanup ImGui resources)
    ~ImGuiContext();
    
    /**
     * @brief Creates ImGui context with GLFW and Vulkan backends
     * 
     * This factory function initializes ImGui, sets up GLFW backend for input,
     * and Vulkan backend for rendering. It also creates necessary Vulkan
     * resources (descriptor pool, render pass) for ImGui rendering.
     * 
     * ⚠️ IMPURE FUNCTION (creates Vulkan resources, modifies global ImGui state)
     * 
     * side effects:
     * - Creates ImGui context (global singleton)
     * - Initializes GLFW backend (hooks into GLFW callbacks)
     * - Initializes Vulkan backend (allocates GPU resources)
     * - Creates descriptor pool (1000 textures capacity)
     * - Creates render pass (compatible with swapchain format)
     * 
     * @param instance Vulkan instance (for validation layers check)
     * @param device Vulkan device (for resource creation)
     * @param window GLFW window (for input backend)
     * @param swapchain Swapchain (for render pass format compatibility)
     * 
     * @return ImGuiContext on success, Error on failure
     * @retval CORE_INITIALIZATION_FAILED if ImGui context creation fails
     * @retval VULKAN_RESOURCE_CREATION_FAILED if descriptor pool/render pass fails
     * 
     * @pre Vulkan device must be initialized
     * @pre GLFW window must be created
     * @pre Swapchain must be valid
     * @post ImGui is ready for use (can call ImGui::* functions)
     * @post begin_frame/end_frame/render can be called each frame
     * 
     * @note This is a factory function (returns Result<T>)
     * @note ImGui context is unique (only one per application)
     * @warning Do not create multiple ImGuiContext instances
     * 
     * example:
     * @code
     * auto imgui = ImGuiContext::create(instance, device, window, swapchain);
     * if (!imgui) {
     *     LOG_ERROR("Failed to create ImGui context: {}", imgui.error());
     *     return;
     * }
     * // use imgui.value() for rendering
     * @endcode
     */
    [[nodiscard]] static auto create(
        const vulkan::Instance& instance,
        const vulkan::Device& device,
        const Window& window,
        const vulkan::Swapchain& swapchain
    ) -> Result<ImGuiContext>;
    
    /**
     * @brief Begins new ImGui frame (call before ImGui::* calls)
     * 
     * This method starts a new ImGui frame by calling NewFrame on GLFW backend,
     * Vulkan backend, and ImGui itself. After calling this, you can use
     * ImGui::Begin, ImGui::Text, etc. to build your UI.
     * 
     * ⚠️ IMPURE FUNCTION (modifies ImGui state, polls input)
     * 
     * side effects:
     * - Polls GLFW input (keyboard, mouse, gamepad)
     * - Updates ImGui internal state (mouse position, key states, etc.)
     * - Prepares ImGui for new frame (clears previous frame data)
     * 
     * @pre ImGui context must be initialized (via create())
     * @post ImGui is ready for widget calls (ImGui::Begin, ImGui::Text, etc.)
     * 
     * @note Must call this exactly once per frame before any ImGui:: calls
     * @note Must be followed by end_frame() and render() same frame
     * @warning Calling ImGui:: functions without begin_frame is undefined behavior
     * 
     * example:
     * @code
     * imgui_ctx.begin_frame();
     * 
     * ImGui::Begin("Debug");
     * ImGui::Text("Hello LUMA!");
     * ImGui::End();
     * 
     * imgui_ctx.end_frame();
     * @endcode
     */
    auto begin_frame() -> void;
    
    /**
     * @brief Ends ImGui frame (call after ImGui::* calls)
     * 
     * This method finalizes the current ImGui frame by calling Render() and
     * UpdatePlatformWindows(). This generates draw data that will be rendered
     * by the render() method in Vulkan command buffer.
     * 
     * ⚠️ IMPURE FUNCTION (modifies ImGui state, may update OS windows)
     * 
     * side effects:
     * - Finalizes ImGui draw data (vertices, indices, draw commands)
     * - Updates platform windows (if using multi-viewport docking)
     * - May create/destroy OS windows (for docked viewports)
     * 
     * @pre begin_frame() must have been called this frame
     * @pre All ImGui:: widget calls must be complete
     * @post ImGui draw data is ready for render()
     * 
     * @note Must call this exactly once per frame after all ImGui:: calls
     * @note Must call render() afterwards to actually draw to command buffer
     * @warning Calling end_frame() without begin_frame() is undefined behavior
     * 
     * example:
     * @code
     * imgui_ctx.begin_frame();
     * // ... ImGui widget calls ...
     * imgui_ctx.end_frame();
     * 
     * // later, in command buffer recording
     * imgui_ctx.render(cmd);
     * @endcode
     */
    auto end_frame() -> void;
    
    /**
     * @brief Renders ImGui to command buffer (records Vulkan draw commands)
     * 
     * This method records Vulkan commands to draw ImGui to the current
     * swapchain image. Must be called inside command buffer recording
     * (between vkBeginCommandBuffer and vkEndCommandBuffer).
     * 
     * ⚠️ IMPURE FUNCTION (records commands to Vulkan command buffer)
     * 
     * side effects:
     * - Begins render pass (for ImGui rendering)
     * - Records draw commands (vertices, indices, scissors, textures)
     * - Ends render pass
     * - Writes to command buffer (GPU work queued)
     * 
     * @param command_buffer Vulkan command buffer (must be in recording state)
     * @param framebuffer Vulkan framebuffer for current swapchain image
     * @param width Framebuffer width in pixels
     * @param height Framebuffer height in pixels
     * 
     * @pre end_frame() must have been called this frame
     * @pre command_buffer must be in recording state
     * @pre framebuffer must be valid and match swapchain format
     * @post Vulkan commands recorded to draw ImGui
     * 
     * @note Call this during command buffer recording phase
     * @note Framebuffer size must match swapchain image size
     * @warning Must not call this if ImGui has no draw data (skip if no UI)
     * 
     * example:
     * @code
     * vkBeginCommandBuffer(cmd, ...);
     * 
     * // path tracer rendering
     * dispatch_path_tracer(cmd);
     * 
     * // ImGui overlay
     * imgui_ctx.render(cmd, framebuffer, width, height);
     * 
     * vkEndCommandBuffer(cmd);
     * @endcode
     */
    auto render(VkCommandBuffer command_buffer, VkFramebuffer framebuffer, 
                u32 width, u32 height) -> void;
    
    /**
     * @brief Gets ImGui render pass handle
     * 
     * ✨ PURE FUNCTION ✨ (no side effects, just returns handle)
     * 
     * @return VkRenderPass handle used by ImGui
     * 
     * @note Useful for framebuffer creation (must match render pass)
     * @note Render pass is compatible with swapchain format
     */
    [[nodiscard]] auto render_pass() const noexcept -> VkRenderPass {
        return render_pass_;
    }
    
    /**
     * @brief Gets ImGui descriptor pool handle
     * 
     * ✨ PURE FUNCTION ✨ (no side effects, just returns handle)
     * 
     * @return VkDescriptorPool handle used by ImGui
     * 
     * @note Pool has capacity for 1000 textures (should be plenty for UI)
     * @note Can be used for custom ImGui textures if needed
     */
    [[nodiscard]] auto descriptor_pool() const noexcept -> VkDescriptorPool {
        return descriptor_pool_;
    }

private:
    /**
     * @brief Private constructor (use create() factory function)
     * 
     * @param device Vulkan device
     * @param descriptor_pool ImGui descriptor pool
     * @param render_pass ImGui render pass
     */
    ImGuiContext(
        const vulkan::Device& device,
        VkDescriptorPool descriptor_pool,
        VkRenderPass render_pass
    );
    
    const vulkan::Device* device_{nullptr};        ///< Vulkan device (for cleanup)
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE}; ///< Descriptor pool for ImGui textures
    VkRenderPass render_pass_{VK_NULL_HANDLE};    ///< Render pass for ImGui rendering
};

} // namespace luma::editor
