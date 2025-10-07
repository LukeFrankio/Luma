/**
 * @file window.hpp
 * @brief GLFW window wrapper with Vulkan surface integration (windowing made functional uwu)
 * 
 * this file defines the Window class that wraps GLFW window creation and management.
 * it handles window creation, event polling, and Vulkan surface creation for rendering.
 * 
 * we're using GLFW because it's cross-platform and plays nice with Vulkan. the Window
 * class provides RAII semantics (move-only, automatic cleanup) and integrates tightly
 * with our Vulkan Instance for surface creation.
 * 
 * the window is the user's viewport into our path-traced masterpiece. it also handles
 * framebuffer resize events which trigger swapchain recreation (window resizing is
 * actually just functional state transitions uwu)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note uses C++26 features (latest standard, even beta features ok!)
 * @note compiled with GCC 15+ (latest version preferred)
 * @note requires GLFW 3.4+ (latest version with Vulkan support)
 * @note requires CMake 4.1+ (always prefer latest version)
 * 
 * example usage:
 * @code
 * // create window with Vulkan support
 * auto window_result = Window::create("LUMA Engine", 1920, 1080);
 * if (!window_result) {
 *     LOG_ERROR("Failed to create window: {}", window_result.error().message);
 *     return -1;
 * }
 * auto window = std::move(*window_result);
 * 
 * // main loop (functional event processing)
 * while (!window.should_close()) {
 *     window.poll_events();  // pure event polling
 *     // render frame...
 * }
 * // automatic cleanup on scope exit ✨
 * @endcode
 */

#pragma once

#include <luma/core/types.hpp>

#include <vulkan/vulkan.h>

#include <string>
#include <functional>

// forward declare GLFW types (avoid polluting namespace)
struct GLFWwindow;

namespace luma {

/**
 * @class Window
 * @brief RAII wrapper for GLFW window with Vulkan surface integration
 * 
 * the Window class manages a GLFW window and provides methods for window
 * operations, event polling, and Vulkan surface creation. it uses RAII
 * semantics for automatic resource cleanup.
 * 
 * this class is move-only (no copying windows!) and provides a functional
 * interface for querying window state and handling events. window resize
 * events trigger callbacks that can be used to recreate the swapchain.
 * 
 * Design decisions:
 * - move-only semantics (windows are unique resources)
 * - RAII cleanup (GLFW resources automatically freed)
 * - callback-based event handling (functional event processing)
 * - integrated with Vulkan Instance for surface creation
 * 
 * @invariant window_ is valid after successful construction
 * @invariant GLFW is initialized before any Window is created
 * 
 * @note prefer creating one window per application
 * @note window must outlive any Vulkan objects using its surface
 * @warning GLFW must be initialized before creating Window
 * @warning Window must be created on the main thread (GLFW requirement)
 * 
 * example usage:
 * @code
 * auto window = Window::create("My App", 1280, 720);
 * if (window) {
 *     window->set_resize_callback([](int w, int h) {
 *         LOG_INFO("Window resized to {}x{}", w, h);
 *     });
 *     
 *     while (!window->should_close()) {
 *         window->poll_events();  // process events
 *         // render...
 *     }
 * }
 * @endcode
 */
class Window {
public:
    /// @brief Callback function type for framebuffer resize events
    /// @param width New framebuffer width in pixels
    /// @param height New framebuffer height in pixels
    using ResizeCallback = std::function<void(int width, int height)>;

    /**
     * @brief Create a new window with Vulkan support
     * 
     * ✨ PURE FACTORY FUNCTION ✨
     * 
     * this function creates a new GLFW window with Vulkan support enabled.
     * it initializes GLFW if needed (first call only), creates the window,
     * and returns a Window instance wrapped in Result for error handling.
     * 
     * the function is pure in the sense that it has no side effects on existing
     * state (GLFW initialization is a one-time setup, not a side effect). it
     * returns a new Window object or an error.
     * 
     * purity characteristics:
     * - deterministic for same inputs (same title/width/height = same window config)
     * - GLFW initialization is idempotent (safe to call multiple times)
     * - no exceptions thrown (uses Result<T> for error handling)
     * - referentially transparent (can be called multiple times safely)
     * 
     * @param[in] title Window title string (displayed in title bar)
     * @param[in] width Window width in pixels (screen coordinates)
     * @param[in] height Window height in pixels (screen coordinates)
     * 
     * @return Result<Window> containing Window on success, Error on failure
     * @retval Window Successfully created window instance
     * @retval Error GLFW initialization or window creation failed
     * 
     * @pre title is valid UTF-8 string
     * @pre width > 0 and height > 0 (must be positive dimensions)
     * @post on success, GLFW is initialized and window is created
     * @post on failure, no resources are leaked (RAII cleanup)
     * 
     * @throws none (noexcept, uses Result<T> for errors)
     * 
     * @note first call initializes GLFW library (one-time setup)
     * @note subsequent calls reuse GLFW instance (singleton pattern)
     * @warning must be called on main thread (GLFW requirement)
     * @warning do not create windows before calling glfwInit() externally
     * 
     * @complexity O(1) time (window creation is constant time)
     * @complexity O(1) space (window handle allocation)
     * 
     * @see poll_events() for event processing
     * @see should_close() for main loop condition
     * 
     * example (basic usage):
     * @code
     * auto window = Window::create("LUMA Engine", 1920, 1080);
     * if (!window) {
     *     LOG_ERROR("Window creation failed: {}", window.error().message);
     *     return -1;
     * }
     * // window is now ready for rendering
     * @endcode
     * 
     * example (handling errors functionally):
     * @code
     * auto create_and_run = []() -> Result<void> {
     *     auto window = Window::create("App", 1280, 720);
     *     if (!window) return std::unexpected(window.error());
     *     
     *     // use window...
     *     return {};
     * };
     * @endcode
     */
    [[nodiscard]] static auto create(const std::string& title, int width, int height) -> Result<Window>;

    /**
     * @brief Initialize GLFW library (call before creating any windows)
     * 
     * ✨ SEMI-PURE FUNCTION ✨
     * 
     * this function initializes the GLFW library. it must be called before
     * creating any windows. subsequent calls are safe (idempotent).
     * 
     * this is semi-pure because it modifies global GLFW state, but it's
     * idempotent (calling multiple times has same effect as calling once).
     * 
     * @return Result<void> indicating success or failure
     * @retval void GLFW initialized successfully
     * @retval Error GLFW initialization failed
     * 
     * @pre none (can be called any time)
     * @post on success, GLFW is initialized and ready for window creation
     * 
     * @throws none (noexcept, uses Result<T>)
     * 
     * @note this is called automatically by create() if needed
     * @note safe to call multiple times (idempotent operation)
     * @warning must be called on main thread (GLFW requirement)
     * 
     * example:
     * @code
     * if (auto result = Window::init(); !result) {
     *     LOG_ERROR("GLFW init failed: {}", result.error().message);
     *     return -1;
     * }
     * @endcode
     */
    [[nodiscard]] static auto init() -> Result<void>;

    /**
     * @brief Terminate GLFW library (cleanup, call at program exit)
     * 
     * ⚠️ IMPURE FUNCTION (modifies global state)
     * 
     * this function terminates the GLFW library and cleans up all resources.
     * call this at program exit, after all windows are destroyed.
     * 
     * side effects:
     * - terminates GLFW library (global state modification)
     * - invalidates all window handles
     * 
     * @note automatically called at program exit (via atexit)
     * @note safe to call multiple times (idempotent)
     * @warning destroys ALL windows (even those not managed by this class)
     * @warning call only after all Window objects are destroyed
     * 
     * example:
     * @code
     * {
     *     auto window = Window::create("App", 800, 600);
     *     // use window...
     * }  // window destroyed here
     * Window::terminate();  // cleanup GLFW
     * @endcode
     */
    static auto terminate() -> void;

    /**
     * @brief Default constructor (creates invalid window, use create() instead)
     * 
     * constructs an invalid Window instance. this is primarily for move semantics.
     * prefer using create() to construct valid windows.
     * 
     * @note window_ is nullptr after default construction
     * @note this constructor exists for move semantics support
     */
    Window() noexcept = default;

    /**
     * @brief Destructor (cleans up GLFW window)
     * 
     * destroys the GLFW window and releases resources. this is called
     * automatically when Window goes out of scope (RAII cleanup).
     * 
     * @note safe to call even if window_ is nullptr
     * @note automatically releases Vulkan surface (handled by Instance)
     */
    ~Window() noexcept;

    // move semantics (windows are unique resources)
    Window(Window&& other) noexcept;
    auto operator=(Window&& other) noexcept -> Window&;

    // delete copy semantics (windows cannot be copied)
    Window(const Window&) = delete;
    auto operator=(const Window&) = delete;

    /**
     * @brief Check if window should close (user clicked X or pressed Escape)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * this function queries the window's close flag, which is set when the
     * user clicks the close button or presses Escape. use this as the
     * condition for your main loop.
     * 
     * purity characteristics:
     * - no side effects (only reads GLFW state)
     * - deterministic (same window state = same result)
     * - referentially transparent
     * - const-qualified (doesn't modify Window)
     * 
     * @return true if window should close, false otherwise
     * @retval true User requested window close
     * @retval false Window should remain open
     * 
     * @pre window is valid (window_ != nullptr)
     * @post window state unchanged
     * 
     * @throws none (noexcept)
     * 
     * @complexity O(1) time (single GLFW call)
     * 
     * example (main loop):
     * @code
     * while (!window.should_close()) {
     *     window.poll_events();
     *     render_frame();
     * }
     * @endcode
     */
    [[nodiscard]] auto should_close() const noexcept -> bool;

    /**
     * @brief Poll window events (keyboard, mouse, resize, etc.)
     * 
     * ⚠️ IMPURE FUNCTION (has side effects)
     * 
     * this function processes all pending window events (keyboard input,
     * mouse movement, resize, etc.) and invokes registered callbacks.
     * call this once per frame in your main loop.
     * 
     * side effects:
     * - invokes resize callbacks if framebuffer size changed
     * - updates input state (keyboard/mouse)
     * - may trigger window close flag
     * 
     * @note call this once per frame before rendering
     * @note callbacks are invoked on calling thread (usually main thread)
     * @warning must be called on main thread (GLFW requirement)
     * 
     * example:
     * @code
     * while (!window.should_close()) {
     *     window.poll_events();  // process input events
     *     update_game_state();
     *     render_frame();
     * }
     * @endcode
     */
    auto poll_events() const noexcept -> void;

    /**
     * @brief Get current window width in pixels
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Current window width in pixels (screen coordinates)
     * 
     * @note may differ from framebuffer width on high-DPI displays
     * @see framebuffer_width() for actual pixel count
     */
    [[nodiscard]] auto width() const noexcept -> int;

    /**
     * @brief Get current window height in pixels
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Current window height in pixels (screen coordinates)
     * 
     * @note may differ from framebuffer height on high-DPI displays
     * @see framebuffer_height() for actual pixel count
     */
    [[nodiscard]] auto height() const noexcept -> int;

    /**
     * @brief Get current framebuffer width in pixels
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the actual pixel width of the framebuffer, which may differ
     * from window width on high-DPI displays (e.g., Retina displays).
     * use this for Vulkan viewport/scissor setup.
     * 
     * @return Framebuffer width in actual pixels
     * 
     * @note this is what you want for Vulkan rendering (actual pixel count)
     * @note may be 2x or more than window width on high-DPI displays
     */
    [[nodiscard]] auto framebuffer_width() const noexcept -> int;

    /**
     * @brief Get current framebuffer height in pixels
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the actual pixel height of the framebuffer, which may differ
     * from window height on high-DPI displays (e.g., Retina displays).
     * use this for Vulkan viewport/scissor setup.
     * 
     * @return Framebuffer height in actual pixels
     * 
     * @note this is what you want for Vulkan rendering (actual pixel count)
     * @note may be 2x or more than window height on high-DPI displays
     */
    [[nodiscard]] auto framebuffer_height() const noexcept -> int;

    /**
     * @brief Check if window is currently minimized (iconified)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if window is minimized, false otherwise
     * 
     * @note useful for skipping rendering when minimized (performance optimization)
     * @note window may have zero framebuffer size when minimized
     */
    [[nodiscard]] auto is_minimized() const noexcept -> bool;

    /**
     * @brief Wait until window is not minimized
     * 
     * ⚠️ IMPURE FUNCTION (blocks thread)
     * 
     * this function blocks the calling thread until the window is no longer
     * minimized. use this to avoid wasting CPU/GPU resources when the window
     * is iconified.
     * 
     * side effects:
     * - blocks calling thread (potentially indefinitely)
     * - polls events while waiting
     * 
     * @note useful for pausing rendering when window is minimized
     * @warning blocks thread until window is restored
     */
    auto wait_while_minimized() const noexcept -> void;

    /**
     * @brief Set framebuffer resize callback
     * 
     * ⚠️ IMPURE FUNCTION (modifies callback state)
     * 
     * registers a callback function that will be invoked when the framebuffer
     * size changes (window resize, maximize, etc.). use this to recreate the
     * swapchain when the window size changes.
     * 
     * side effects:
     * - replaces existing resize callback (if any)
     * - stores callback in Window state
     * 
     * @param[in] callback Function to call on resize (signature: void(int, int))
     * 
     * @note callback is invoked on main thread during poll_events()
     * @note callback receives framebuffer size, not window size
     * @warning callback must be fast (don't block event processing)
     * 
     * example:
     * @code
     * window.set_resize_callback([&swapchain](int w, int h) {
     *     LOG_INFO("Framebuffer resized to {}x{}", w, h);
     *     swapchain.recreate(w, h);
     * });
     * @endcode
     */
    auto set_resize_callback(ResizeCallback callback) -> void;

    /**
     * @brief Create Vulkan surface for this window
     * 
     * ✨ SEMI-PURE FUNCTION ✨
     * 
     * creates a VkSurfaceKHR for this window using the provided Vulkan instance.
     * the surface is used for swapchain creation and presentation.
     * 
     * this is semi-pure because it creates a new Vulkan object (side effect),
     * but it's deterministic and doesn't modify existing state.
     * 
     * @param[in] instance Vulkan instance handle to use for surface creation
     * 
     * @return Result<VkSurfaceKHR> containing surface handle on success
     * @retval VkSurfaceKHR Successfully created surface handle
     * @retval Error Surface creation failed
     * 
     * @pre window is valid (window_ != nullptr)
     * @pre instance is valid Vulkan instance handle
     * @post on success, surface is created and owned by instance
     * 
     * @note surface is owned by Vulkan instance (destroyed with instance)
     * @note only call this once per window (surface creation is expensive)
     * @warning surface must be destroyed before window
     * 
     * example:
     * @code
     * auto window = Window::create("LUMA", 1920, 1080);
     * auto surface = window->create_surface(instance_handle);
     * if (!surface) {
     *     LOG_ERROR("Surface creation failed: {}", surface.error().message);
     * }
     * @endcode
     */
    [[nodiscard]] auto create_surface(VkInstance instance) const -> Result<VkSurfaceKHR>;

    /**
     * @brief Get raw GLFW window handle (for advanced usage)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the underlying GLFWwindow pointer. use this only if you need
     * direct access to GLFW API (prefer using Window methods when possible).
     * 
     * @return GLFWwindow* Raw GLFW window handle
     * 
     * @note prefer using Window methods over direct GLFW access
     * @note handle is valid for lifetime of Window object
     * @warning do not call glfwDestroyWindow() on this handle
     */
    [[nodiscard]] auto handle() const noexcept -> GLFWwindow* { return window_; }

private:
    /**
     * @brief Private constructor (use create() factory function)
     * 
     * @param window GLFW window handle (takes ownership)
     */
    explicit Window(GLFWwindow* window) noexcept;

    /**
     * @brief Static framebuffer size callback (GLFW callback convention)
     * 
     * this is the C-style callback that GLFW invokes on resize. it forwards
     * the call to the Window instance's resize callback.
     * 
     * @param window GLFW window handle
     * @param width New framebuffer width
     * @param height New framebuffer height
     */
    static auto framebuffer_size_callback(GLFWwindow* window, int width, int height) -> void;

    GLFWwindow* window_ = nullptr;  ///< GLFW window handle (nullptr if invalid)
    ResizeCallback resize_callback_;  ///< User-provided resize callback (optional)
};

}  // namespace luma
