/**
 * @file input.cpp
 * @brief Implementation of keyboard and mouse input handling (functional input queries uwu)
 * 
 * implements the Input class for polling keyboard and mouse state from GLFW.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/input/input.hpp>
#include <luma/core/logging.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <unordered_map>

namespace luma {

// ============================================================================
// Static input instance registry (for GLFW callbacks)
// ============================================================================

namespace {
    /// @brief Map GLFW window to Input instance (for scroll callback)
    std::unordered_map<GLFWwindow*, Input*> input_registry;
}

// ============================================================================
// Factory function
// ============================================================================

auto Input::create(const Window& window) -> Input {
    // set up scroll callback (Input will register itself after construction)
    glfwSetScrollCallback(window.handle(), scroll_callback);
    
    LOG_INFO("Input system initialized");
    return Input{window};
}

// ============================================================================
// Constructor
// ============================================================================

Input::Input(const Window& window) noexcept
    : window_{&window}
{
    // initialize state arrays to false/zero
    current_keys_.fill(false);
    previous_keys_.fill(false);
    current_buttons_.fill(false);
    previous_buttons_.fill(false);
    
    // register this instance in the registry for callbacks
    if (window_->handle()) {
        input_registry[window_->handle()] = this;
    }
}

// ============================================================================
// Move semantics (must update registry)
// ============================================================================

Input::Input(Input&& other) noexcept
    : window_{other.window_}
    , current_keys_{std::move(other.current_keys_)}
    , previous_keys_{std::move(other.previous_keys_)}
    , current_buttons_{std::move(other.current_buttons_)}
    , previous_buttons_{std::move(other.previous_buttons_)}
    , current_mouse_pos_{other.current_mouse_pos_}
    , previous_mouse_pos_{other.previous_mouse_pos_}
    , mouse_delta_{other.mouse_delta_}
    , scroll_delta_{other.scroll_delta_}
{
    other.window_ = nullptr;
    
    // update registry to point to new location
    if (window_ && window_->handle()) {
        input_registry[window_->handle()] = this;
    }
}

auto Input::operator=(Input&& other) noexcept -> Input& {
    if (this != &other) {
        // unregister old window
        if (window_ && window_->handle()) {
            input_registry.erase(window_->handle());
        }
        
        // move from other
        window_ = other.window_;
        current_keys_ = std::move(other.current_keys_);
        previous_keys_ = std::move(other.previous_keys_);
        current_buttons_ = std::move(other.current_buttons_);
        previous_buttons_ = std::move(other.previous_buttons_);
        current_mouse_pos_ = other.current_mouse_pos_;
        previous_mouse_pos_ = other.previous_mouse_pos_;
        mouse_delta_ = other.mouse_delta_;
        scroll_delta_ = other.scroll_delta_;
        
        other.window_ = nullptr;
        
        // register new window
        if (window_ && window_->handle()) {
            input_registry[window_->handle()] = this;
        }
    }
    return *this;
}

// ============================================================================
// Update function (polls GLFW state)
// ============================================================================

auto Input::update() -> void {
    if (!window_ || !window_->handle()) {
        return;  // invalid window, skip update
    }

    GLFWwindow* glfw_window = window_->handle();

    // store previous frame state
    previous_keys_ = current_keys_;
    previous_buttons_ = current_buttons_;
    previous_mouse_pos_ = current_mouse_pos_;

    // poll keyboard state
    for (int key = 0; key < NUM_KEYS; ++key) {
        int state = glfwGetKey(glfw_window, key);
        current_keys_[key] = (state == GLFW_PRESS || state == GLFW_REPEAT);
    }

    // poll mouse button state
    for (int button = 0; button < NUM_BUTTONS; ++button) {
        int state = glfwGetMouseButton(glfw_window, button);
        current_buttons_[button] = (state == GLFW_PRESS);
    }

    // poll mouse position
    double xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos(glfw_window, &xpos, &ypos);
    current_mouse_pos_ = vec2{static_cast<f32>(xpos), static_cast<f32>(ypos)};

    // calculate mouse delta
    mouse_delta_ = current_mouse_pos_ - previous_mouse_pos_;

    // reset scroll delta after update (it accumulates during frame via callback)
    // note: we reset AFTER reading, so scroll_delta() returns accumulated value
    // for this frame, then clears for next frame
    scroll_delta_ = vec2{0.0f, 0.0f};
}

// ============================================================================
// Keyboard query functions (pure, const, noexcept)
// ============================================================================

auto Input::is_key_pressed(int key) const noexcept -> bool {
    if (key < 0 || key >= NUM_KEYS) {
        return false;  // invalid key code
    }
    return current_keys_[key];
}

auto Input::is_key_just_pressed(int key) const noexcept -> bool {
    if (key < 0 || key >= NUM_KEYS) {
        return false;
    }
    // rising edge: was not pressed, now is pressed
    return current_keys_[key] && !previous_keys_[key];
}

auto Input::is_key_just_released(int key) const noexcept -> bool {
    if (key < 0 || key >= NUM_KEYS) {
        return false;
    }
    // falling edge: was pressed, now is not pressed
    return !current_keys_[key] && previous_keys_[key];
}

// ============================================================================
// Mouse button query functions (pure, const, noexcept)
// ============================================================================

auto Input::is_mouse_button_pressed(int button) const noexcept -> bool {
    if (button < 0 || button >= NUM_BUTTONS) {
        return false;
    }
    return current_buttons_[button];
}

auto Input::is_mouse_button_just_pressed(int button) const noexcept -> bool {
    if (button < 0 || button >= NUM_BUTTONS) {
        return false;
    }
    // rising edge
    return current_buttons_[button] && !previous_buttons_[button];
}

auto Input::is_mouse_button_just_released(int button) const noexcept -> bool {
    if (button < 0 || button >= NUM_BUTTONS) {
        return false;
    }
    // falling edge
    return !current_buttons_[button] && previous_buttons_[button];
}

// ============================================================================
// Mouse position/delta query functions (pure, const, noexcept)
// ============================================================================

auto Input::mouse_position() const noexcept -> vec2 {
    return current_mouse_pos_;
}

auto Input::mouse_delta() const noexcept -> vec2 {
    return mouse_delta_;
}

auto Input::mouse_scroll() const noexcept -> vec2 {
    return scroll_delta_;
}

// ============================================================================
// Cursor mode (impure, modifies cursor state)
// ============================================================================

auto Input::set_cursor_mode(int mode) const -> void {
    if (window_ && window_->handle()) {
        glfwSetInputMode(window_->handle(), GLFW_CURSOR, mode);
    }
}

// ============================================================================
// GLFW callbacks
// ============================================================================

auto Input::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void {
    // look up Input instance from registry
    auto it = input_registry.find(window);
    if (it != input_registry.end() && it->second) {
        // accumulate scroll delta (multiple events sum over frame)
        // this persists until next update() call, where it's reset
        it->second->scroll_delta_.x += static_cast<f32>(xoffset);
        it->second->scroll_delta_.y += static_cast<f32>(yoffset);
    }
}

}  // namespace luma
