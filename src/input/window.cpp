/**
 * @file window.cpp
 * @brief Implementation of GLFW window wrapper (windowing made functional uwu)
 * 
 * implements the Window class for GLFW window management and Vulkan surface creation.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/input/window.hpp>
#include <luma/core/logging.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace luma {

// ============================================================================
// Static GLFW initialization state
// ============================================================================

namespace {
    /// @brief Track if GLFW has been initialized (singleton state)
    bool glfw_initialized = false;
}

// ============================================================================
// Static member functions
// ============================================================================

auto Window::init() -> Result<void> {
    if (glfw_initialized) {
        return {};  // already initialized (idempotent)
    }

    LOG_INFO("Initializing GLFW...");

    // set error callback before init
    glfwSetErrorCallback([](int error, const char* description) {
        LOG_ERROR("GLFW error {}: {}", error, description);
    });

    if (!glfwInit()) {
        return std::unexpected(Error{
            ErrorCode::INITIALIZATION_FAILED,
            "Failed to initialize GLFW"
        });
    }

    glfw_initialized = true;
    LOG_INFO("GLFW initialized successfully");

    // register cleanup at exit
    std::atexit([]() {
        if (glfw_initialized) {
            glfwTerminate();
            glfw_initialized = false;
        }
    });

    return {};
}

auto Window::terminate() -> void {
    if (glfw_initialized) {
        LOG_INFO("Terminating GLFW...");
        glfwTerminate();
        glfw_initialized = false;
    }
}

auto Window::create(const std::string& title, int width, int height) -> Result<Window> {
    // ensure GLFW is initialized
    if (auto result = init(); !result) {
        return std::unexpected(result.error());
    }

    // configure GLFW for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    // allow window resizing
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    LOG_INFO("Creating window: \"{}\" ({}x{})", title, width, height);

    // create window
    GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        return std::unexpected(Error{
            ErrorCode::INITIALIZATION_FAILED,
            "Failed to create GLFW window"
        });
    }

    LOG_INFO("Window created successfully");
    return Window{window};
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

Window::Window(GLFWwindow* window) noexcept
    : window_{window}
{
    // set user pointer for callbacks
    if (window_) {
        glfwSetWindowUserPointer(window_, this);
        
        // register framebuffer size callback
        glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    }
}

Window::~Window() noexcept {
    if (window_) {
        LOG_INFO("Destroying window");
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}

// ============================================================================
// Move semantics
// ============================================================================

Window::Window(Window&& other) noexcept
    : window_{other.window_}
    , resize_callback_{std::move(other.resize_callback_)}
{
    other.window_ = nullptr;
    
    // update user pointer for callbacks
    if (window_) {
        glfwSetWindowUserPointer(window_, this);
    }
}

auto Window::operator=(Window&& other) noexcept -> Window& {
    if (this != &other) {
        // clean up existing window
        if (window_) {
            glfwDestroyWindow(window_);
        }
        
        // move from other
        window_ = other.window_;
        resize_callback_ = std::move(other.resize_callback_);
        other.window_ = nullptr;
        
        // update user pointer for callbacks
        if (window_) {
            glfwSetWindowUserPointer(window_, this);
        }
    }
    return *this;
}

// ============================================================================
// Query functions (pure, const, noexcept)
// ============================================================================

auto Window::should_close() const noexcept -> bool {
    return window_ && glfwWindowShouldClose(window_);
}

auto Window::poll_events() const noexcept -> void {
    glfwPollEvents();
}

auto Window::width() const noexcept -> int {
    int w = 0, h = 0;
    if (window_) {
        glfwGetWindowSize(window_, &w, &h);
    }
    return w;
}

auto Window::height() const noexcept -> int {
    int w = 0, h = 0;
    if (window_) {
        glfwGetWindowSize(window_, &w, &h);
    }
    return h;
}

auto Window::framebuffer_width() const noexcept -> int {
    int w = 0, h = 0;
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
    }
    return w;
}

auto Window::framebuffer_height() const noexcept -> int {
    int w = 0, h = 0;
    if (window_) {
        glfwGetFramebufferSize(window_, &w, &h);
    }
    return h;
}

auto Window::is_minimized() const noexcept -> bool {
    if (!window_) return false;
    return glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0;
}

auto Window::wait_while_minimized() const noexcept -> void {
    while (is_minimized()) {
        glfwWaitEvents();  // block until window event
    }
}

// ============================================================================
// Callback registration (impure, modifies state)
// ============================================================================

auto Window::set_resize_callback(ResizeCallback callback) -> void {
    resize_callback_ = std::move(callback);
}

// ============================================================================
// Static callback forwarders
// ============================================================================

auto Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) -> void {
    // retrieve Window instance from user pointer
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->resize_callback_) {
        self->resize_callback_(width, height);
    }
}

// ============================================================================
// Vulkan surface creation
// ============================================================================

auto Window::create_surface(VkInstance instance) const -> Result<VkSurfaceKHR> {
    if (!window_) {
        return std::unexpected(Error{
            ErrorCode::INVALID_ARGUMENT,
            "Cannot create surface for invalid window"
        });
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(
        instance,
        window_,
        nullptr,  // no custom allocator
        &surface
    );

    if (result != VK_SUCCESS) {
        return std::unexpected(Error{
            ErrorCode::VULKAN_OPERATION_FAILED,
            std::format("Failed to create Vulkan surface: {}", static_cast<int>(result))
        });
    }

    LOG_INFO("Vulkan surface created successfully");
    return surface;
}

}  // namespace luma
