/**
 * @file test_input.cpp
 * @brief Test program for luma_input module (window + keyboard + mouse)
 * 
 * simple test program that creates a window and logs input events.
 * demonstrates the Input system with functional query interface.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/core/logging.hpp>
#include <luma/core/time.hpp>
#include <luma/input/window.hpp>
#include <luma/input/input.hpp>

#include <GLFW/glfw3.h>  // for key codes

using namespace luma;

auto main() -> int {
    // initialize logging
    auto log_result = Logger::instance().initialize();
    if (!log_result) {
        // can't log error if logging failed, just print to stderr
        return -1;
    }
    
    LOG_INFO("=== LUMA Input Module Test ===");
    
    // create window
    auto window_result = Window::create("LUMA Input Test", 1280, 720);
    if (!window_result) {
        LOG_ERROR("Failed to create window: {}", window_result.error().message);
        return -1;
    }
    auto window = std::move(*window_result);
    
    LOG_INFO("Window created successfully ({}x{})", window.width(), window.height());
    
    // set up resize callback
    window.set_resize_callback([](int width, int height) {
        LOG_INFO("Window resized to {}x{}", width, height);
    });
    
    // create input manager
    auto input = Input::create(window);
    
    // create timer for FPS tracking
    Timer timer;
    FPSCounter fps_counter;
    
    LOG_INFO("Entering main loop... (press ESC to exit, WASD to test keyboard, move mouse)");
    
    // main loop
    while (!window.should_close()) {
        // poll events and update input
        window.poll_events();
        input.update();
        
        // check for exit
        if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
            LOG_INFO("ESC pressed, exiting...");
            break;
        }
        
        // test keyboard input (WASD movement)
        if (input.is_key_pressed(GLFW_KEY_W)) {
            LOG_INFO("W pressed (move forward)");
        }
        if (input.is_key_pressed(GLFW_KEY_S)) {
            LOG_INFO("S pressed (move backward)");
        }
        if (input.is_key_pressed(GLFW_KEY_A)) {
            LOG_INFO("A pressed (move left)");
        }
        if (input.is_key_pressed(GLFW_KEY_D)) {
            LOG_INFO("D pressed (move right)");
        }
        
        // test edge detection
        if (input.is_key_just_pressed(GLFW_KEY_SPACE)) {
            LOG_INFO("SPACE just pressed (jump!)");
        }
        if (input.is_key_just_released(GLFW_KEY_SPACE)) {
            LOG_INFO("SPACE just released");
        }
        
        // test mouse buttons
        if (input.is_mouse_button_just_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
            auto pos = input.mouse_position();
            LOG_INFO("Left mouse button pressed at ({:.1f}, {:.1f})", pos.x, pos.y);
        }
        if (input.is_mouse_button_just_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            auto pos = input.mouse_position();
            LOG_INFO("Right mouse button pressed at ({:.1f}, {:.1f})", pos.x, pos.y);
        }
        
        // test mouse movement (only log if delta is significant)
        auto delta = input.mouse_delta();
        if (glm::length(delta) > 5.0f) {
            LOG_INFO("Mouse moved: delta = ({:.1f}, {:.1f})", delta.x, delta.y);
        }
        
        // test scroll
        auto scroll = input.mouse_scroll();
        if (scroll.y != 0.0f) {
            LOG_INFO("Mouse scrolled: {:.1f}", scroll.y);
        }
        
        // update timer and FPS counter
        auto dt = timer.tick();
        fps_counter.update(dt);
        
        // log FPS every 2 seconds
        static f32 time_since_fps_log = 0.0f;
        time_since_fps_log += dt;
        if (time_since_fps_log >= 2.0f) {
            LOG_INFO("FPS: {:.1f} (frame time: {:.2f} ms)", 
                     fps_counter.get_fps(), dt * 1000.0f);
            time_since_fps_log = 0.0f;
        }
        
        // optional: sleep to limit CPU usage (not needed for real rendering)
        // std::this_thread::sleep_for(std::chrono::milliseconds(16));  // ~60 FPS
    }
    
    LOG_INFO("=== Test completed successfully ===");
    
    return 0;
}
