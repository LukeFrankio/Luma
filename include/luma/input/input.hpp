/**
 * @file input.hpp
 * @brief Keyboard and mouse input handling (functional input queries uwu)
 * 
 * this file defines the Input class that provides a functional interface for
 * querying keyboard and mouse state. it polls GLFW input state and provides
 * pure query functions for checking key presses, mouse position, and button states.
 * 
 * the Input system is designed to be polled once per frame, with all state queries
 * being pure functions (no side effects, just reading current state). this makes
 * input handling predictable and composable with other systems.
 * 
 * we track both current and previous frame state to detect key press/release events
 * (rising edge / falling edge detection). this enables just-pressed and just-released
 * queries, which are essential for discrete input actions (jumping, menu navigation, etc.)
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
 * auto input = Input::create(window);
 * 
 * while (!window.should_close()) {
 *     window.poll_events();
 *     input.update();  // poll input state
 *     
 *     // pure query functions (no side effects)
 *     if (input.is_key_pressed(GLFW_KEY_ESCAPE)) {
 *         break;  // exit game
 *     }
 *     
 *     if (input.is_key_just_pressed(GLFW_KEY_SPACE)) {
 *         player.jump();  // discrete action
 *     }
 *     
 *     // mouse queries (also pure)
 *     auto [x, y] = input.mouse_position();
 *     auto delta = input.mouse_delta();
 *     
 *     // functional composition ftw
 *     auto move_direction = [&]() -> vec2 {
 *         vec2 dir{0.0f, 0.0f};
 *         if (input.is_key_pressed(GLFW_KEY_W)) dir.y += 1.0f;
 *         if (input.is_key_pressed(GLFW_KEY_S)) dir.y -= 1.0f;
 *         if (input.is_key_pressed(GLFW_KEY_A)) dir.x -= 1.0f;
 *         if (input.is_key_pressed(GLFW_KEY_D)) dir.x += 1.0f;
 *         return glm::normalize(dir);
 *     };
 * }
 * @endcode
 */

#pragma once

#include <luma/core/types.hpp>
#include <luma/core/math.hpp>
#include <luma/input/window.hpp>

#include <array>

namespace luma {

/**
 * @class Input
 * @brief Keyboard and mouse input state manager with functional query interface
 * 
 * the Input class tracks keyboard and mouse state and provides pure query
 * functions for checking input state. it polls GLFW once per frame (via update())
 * and stores state for fast queries.
 * 
 * this class is designed for efficient polling-based input, with all queries
 * being O(1) lookups into state arrays. it tracks previous frame state to
 * enable edge detection (just-pressed, just-released events).
 * 
 * Design decisions:
 * - polling-based (not event-based) for predictable frame-by-frame state
 * - pure query functions (no side effects, just reading state)
 * - edge detection for discrete actions (just-pressed/just-released)
 * - mouse delta tracking for camera control
 * - move-only semantics (input state is unique to window)
 * 
 * @invariant window reference is valid throughout Input lifetime
 * @invariant state arrays are sized for all GLFW key/button codes
 * 
 * @note prefer polling input once per frame (call update() once)
 * @note all query functions are pure (const, noexcept, no side effects)
 * @warning update() must be called after window.poll_events()
 * @warning window must outlive Input instance
 * 
 * example usage:
 * @code
 * auto input = Input::create(window);
 * 
 * // game loop
 * while (!window.should_close()) {
 *     window.poll_events();
 *     input.update();  // poll input (once per frame)
 *     
 *     // discrete actions (edge-triggered)
 *     if (input.is_key_just_pressed(GLFW_KEY_SPACE)) {
 *         fire_weapon();
 *     }
 *     
 *     // continuous actions (level-triggered)
 *     if (input.is_key_pressed(GLFW_KEY_W)) {
 *         move_forward(delta_time);
 *     }
 *     
 *     // mouse camera control
 *     auto delta = input.mouse_delta();
 *     camera.rotate(delta.x, delta.y);
 * }
 * @endcode
 */
class Input {
public:
    /**
     * @brief Create Input manager for a window
     * 
     * ✨ PURE FACTORY FUNCTION ✨
     * 
     * creates an Input instance that polls input state from the given window.
     * the Input stores a reference to the window (window must outlive Input).
     * 
     * purity characteristics:
     * - deterministic (same window = same Input configuration)
     * - no side effects on existing state
     * - referentially transparent
     * - no exceptions (returns Input directly, not Result<Input>)
     * 
     * @param[in] window Window to poll input from (must outlive Input)
     * 
     * @return Input instance configured for the given window
     * 
     * @pre window is valid (window.handle() != nullptr)
     * @post Input is ready for update() calls
     * 
     * @note Input stores reference to window (window must outlive Input)
     * @note call update() after window.poll_events() each frame
     * 
     * @complexity O(1) time (just stores reference)
     * @complexity O(1) space (fixed-size state arrays)
     * 
     * example:
     * @code
     * auto window = Window::create("Game", 1920, 1080);
     * auto input = Input::create(*window);
     * @endcode
     */
    [[nodiscard]] static auto create(const Window& window) -> Input;

    /**
     * @brief Default constructor (creates invalid Input, use create() instead)
     * 
     * constructs an invalid Input instance. this exists for move semantics.
     * prefer using create() to construct valid Input instances.
     * 
     * @note window reference is nullptr after default construction
     * @note this constructor exists for move semantics support
     */
    Input() noexcept = default;

    // move semantics (input state is unique to window)
    Input(Input&& other) noexcept;
    auto operator=(Input&& other) noexcept -> Input&;

    // delete copy semantics (input state cannot be copied)
    Input(const Input&) = delete;
    auto operator=(const Input&) = delete;

    /**
     * @brief Update input state for current frame (call once per frame)
     * 
     * ⚠️ IMPURE FUNCTION (modifies internal state)
     * 
     * polls GLFW for current keyboard and mouse state and updates internal
     * state arrays. call this once per frame after window.poll_events().
     * 
     * this function stores previous frame state before updating current state,
     * enabling edge detection (just-pressed / just-released queries).
     * 
     * side effects:
     * - updates current key/button state arrays
     * - updates previous key/button state arrays
     * - updates mouse position and delta
     * 
     * @note call once per frame after window.poll_events()
     * @note all query functions reflect state from last update() call
     * @warning must be called after window.poll_events() for accurate input
     * 
     * @complexity O(n) time where n is number of keys/buttons (typically ~350)
     * @complexity O(1) space (fixed-size state arrays)
     * 
     * example:
     * @code
     * while (!window.should_close()) {
     *     window.poll_events();  // GLFW event polling
     *     input.update();        // update input state
     *     
     *     // now input queries are accurate for this frame
     *     if (input.is_key_pressed(GLFW_KEY_W)) {
     *         // move forward...
     *     }
     * }
     * @endcode
     */
    auto update() -> void;

    /**
     * @brief Check if key is currently pressed (level-triggered)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true if the key is currently held down. use this for continuous
     * actions (movement, shooting, etc.). this is level-triggered: it returns
     * true for every frame the key is held.
     * 
     * purity characteristics:
     * - no side effects (only reads state)
     * - deterministic (same key state = same result)
     * - referentially transparent
     * - const-qualified
     * 
     * @param[in] key GLFW key code (e.g., GLFW_KEY_W, GLFW_KEY_SPACE)
     * 
     * @return true if key is pressed, false otherwise
     * @retval true Key is currently held down
     * @retval false Key is not pressed
     * 
     * @pre key is valid GLFW key code (0-348)
     * @post input state unchanged
     * 
     * @throws none (noexcept)
     * 
     * @complexity O(1) time (array lookup)
     * 
     * @see is_key_just_pressed() for discrete actions
     * 
     * example (continuous movement):
     * @code
     * if (input.is_key_pressed(GLFW_KEY_W)) {
     *     player.move_forward(delta_time);  // move every frame
     * }
     * @endcode
     */
    [[nodiscard]] auto is_key_pressed(int key) const noexcept -> bool;

    /**
     * @brief Check if key was just pressed this frame (edge-triggered, rising edge)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true only on the frame when the key transitions from not-pressed
     * to pressed (rising edge detection). use this for discrete actions (jumping,
     * shooting, menu navigation, etc.).
     * 
     * purity characteristics:
     * - no side effects (only reads current and previous state)
     * - deterministic (same state transition = same result)
     * - referentially transparent
     * - const-qualified
     * 
     * @param[in] key GLFW key code (e.g., GLFW_KEY_SPACE, GLFW_KEY_ENTER)
     * 
     * @return true if key was just pressed this frame, false otherwise
     * @retval true Key transitioned from not-pressed to pressed
     * @retval false Key state unchanged or already pressed
     * 
     * @pre key is valid GLFW key code (0-348)
     * @post input state unchanged
     * 
     * @throws none (noexcept)
     * 
     * @complexity O(1) time (two array lookups)
     * 
     * @see is_key_pressed() for continuous actions
     * @see is_key_just_released() for release detection
     * 
     * example (discrete actions):
     * @code
     * if (input.is_key_just_pressed(GLFW_KEY_SPACE)) {
     *     player.jump();  // only trigger once per key press
     * }
     * 
     * if (input.is_key_just_pressed(GLFW_KEY_ESCAPE)) {
     *     toggle_pause_menu();  // don't spam toggle
     * }
     * @endcode
     */
    [[nodiscard]] auto is_key_just_pressed(int key) const noexcept -> bool;

    /**
     * @brief Check if key was just released this frame (edge-triggered, falling edge)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true only on the frame when the key transitions from pressed
     * to not-pressed (falling edge detection). use this for release actions
     * (charge attacks, hold-and-release mechanics, etc.).
     * 
     * @param[in] key GLFW key code
     * 
     * @return true if key was just released this frame, false otherwise
     * @retval true Key transitioned from pressed to not-pressed
     * @retval false Key state unchanged or already released
     * 
     * @pre key is valid GLFW key code (0-348)
     * @post input state unchanged
     * 
     * @throws none (noexcept)
     * 
     * @complexity O(1) time (two array lookups)
     * 
     * @see is_key_just_pressed() for press detection
     * 
     * example (charge attack):
     * @code
     * if (input.is_key_pressed(GLFW_KEY_SPACE)) {
     *     charge_power += delta_time;  // charge while held
     * }
     * if (input.is_key_just_released(GLFW_KEY_SPACE)) {
     *     fire_charged_shot(charge_power);  // release to fire
     *     charge_power = 0.0f;
     * }
     * @endcode
     */
    [[nodiscard]] auto is_key_just_released(int key) const noexcept -> bool;

    /**
     * @brief Check if mouse button is currently pressed (level-triggered)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true if the mouse button is currently held down. use this for
     * continuous actions (dragging, painting, etc.).
     * 
     * @param[in] button GLFW mouse button code (e.g., GLFW_MOUSE_BUTTON_LEFT)
     * 
     * @return true if button is pressed, false otherwise
     * @retval true Button is currently held down
     * @retval false Button is not pressed
     * 
     * @pre button is valid GLFW mouse button code (0-7)
     * @post input state unchanged
     * 
     * @throws none (noexcept)
     * 
     * @complexity O(1) time (array lookup)
     * 
     * @see is_mouse_button_just_pressed() for discrete clicks
     * 
     * example (continuous dragging):
     * @code
     * if (input.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
     *     auto pos = input.mouse_position();
     *     drag_object_to(pos);  // continuous drag
     * }
     * @endcode
     */
    [[nodiscard]] auto is_mouse_button_pressed(int button) const noexcept -> bool;

    /**
     * @brief Check if mouse button was just pressed this frame (edge-triggered)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true only on the frame when the button transitions from not-pressed
     * to pressed. use this for discrete clicks (UI buttons, shooting, etc.).
     * 
     * @param[in] button GLFW mouse button code
     * 
     * @return true if button was just pressed this frame, false otherwise
     * 
     * @complexity O(1) time (two array lookups)
     * 
     * example (discrete click):
     * @code
     * if (input.is_mouse_button_just_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
     *     auto pos = input.mouse_position();
     *     select_object_at(pos);  // single click selection
     * }
     * @endcode
     */
    [[nodiscard]] auto is_mouse_button_just_pressed(int button) const noexcept -> bool;

    /**
     * @brief Check if mouse button was just released this frame (edge-triggered)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns true only on the frame when the button transitions from pressed
     * to not-pressed. use this for release actions (drag-and-drop, etc.).
     * 
     * @param[in] button GLFW mouse button code
     * 
     * @return true if button was just released this frame, false otherwise
     * 
     * @complexity O(1) time (two array lookups)
     * 
     * example (drag-and-drop):
     * @code
     * if (input.is_mouse_button_just_released(GLFW_MOUSE_BUTTON_LEFT)) {
     *     drop_dragged_object();  // release to drop
     * }
     * @endcode
     */
    [[nodiscard]] auto is_mouse_button_just_released(int button) const noexcept -> bool;

    /**
     * @brief Get current mouse position in screen coordinates
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the current mouse position in screen coordinates (pixels from
     * top-left corner). origin is (0, 0) at top-left, +x is right, +y is down.
     * 
     * @return vec2 containing (x, y) mouse position in pixels
     * 
     * @note coordinates are in screen space (not normalized)
     * @note origin (0, 0) is at top-left corner
     * @note may be outside window bounds if cursor left window
     * 
     * @complexity O(1) time (returns cached value)
     * 
     * example:
     * @code
     * auto [x, y] = input.mouse_position();
     * LOG_INFO("Mouse at ({}, {})", x, y);
     * @endcode
     */
    [[nodiscard]] auto mouse_position() const noexcept -> vec2;

    /**
     * @brief Get mouse movement delta since last frame
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the mouse movement delta (change in position) since the last
     * frame. use this for camera control, rotation, etc.
     * 
     * @return vec2 containing (dx, dy) mouse delta in pixels
     * 
     * @note delta is (0, 0) on first frame (no previous position)
     * @note delta can be large if mouse moved quickly or frame time was long
     * @note useful for camera control (multiply by sensitivity)
     * 
     * @complexity O(1) time (returns cached value)
     * 
     * example (camera rotation):
     * @code
     * auto delta = input.mouse_delta();
     * camera.rotate_yaw(delta.x * sensitivity);
     * camera.rotate_pitch(-delta.y * sensitivity);  // invert Y
     * @endcode
     */
    [[nodiscard]] auto mouse_delta() const noexcept -> vec2;

    /**
     * @brief Get mouse scroll delta since last frame
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * returns the mouse scroll wheel delta since the last frame. positive
     * values indicate scrolling up/away, negative values indicate scrolling
     * down/toward.
     * 
     * @return vec2 containing (dx, dy) scroll delta
     * 
     * @note typically only y component is non-zero (vertical scroll)
     * @note some mice support horizontal scrolling (x component)
     * @note delta is accumulated over frame (multiple scroll events sum)
     * 
     * @complexity O(1) time (returns cached value)
     * 
     * example (zoom):
     * @code
     * auto scroll = input.mouse_scroll();
     * camera.zoom(scroll.y * zoom_speed);
     * @endcode
     */
    [[nodiscard]] auto mouse_scroll() const noexcept -> vec2;

    /**
     * @brief Set cursor mode (normal, hidden, or disabled/captured)
     * 
     * ⚠️ IMPURE FUNCTION (modifies cursor state)
     * 
     * sets the cursor mode. GLFW_CURSOR_NORMAL shows the cursor, GLFW_CURSOR_HIDDEN
     * hides it but allows it to leave the window, GLFW_CURSOR_DISABLED hides and
     * captures it (for FPS-style camera control).
     * 
     * side effects:
     * - changes cursor visibility and capture mode
     * - affects mouse position reporting (disabled mode provides infinite movement)
     * 
     * @param[in] mode GLFW cursor mode (GLFW_CURSOR_NORMAL, HIDDEN, or DISABLED)
     * 
     * @note GLFW_CURSOR_DISABLED is ideal for FPS camera control
     * @note mode persists until changed again
     * 
     * example (FPS mode):
     * @code
     * input.set_cursor_mode(GLFW_CURSOR_DISABLED);  // capture cursor
     * // now mouse provides infinite movement for camera rotation
     * @endcode
     */
    auto set_cursor_mode(int mode) const -> void;

private:
    /**
     * @brief Private constructor (use create() factory function)
     * 
     * @param window Window to poll input from
     */
    explicit Input(const Window& window) noexcept;

    /**
     * @brief GLFW scroll callback (forwards to Input instance)
     * 
     * @param window GLFW window handle
     * @param xoffset Horizontal scroll offset
     * @param yoffset Vertical scroll offset
     */
    static auto scroll_callback(GLFWwindow* window, double xoffset, double yoffset) -> void;

    const Window* window_ = nullptr;  ///< Window to poll input from (must outlive Input)

    // keyboard state (GLFW_KEY_LAST is 348 as of GLFW 3.4)
    static constexpr int NUM_KEYS = 512;  // generous buffer for future GLFW versions
    std::array<bool, NUM_KEYS> current_keys_{};   ///< Current frame key state
    std::array<bool, NUM_KEYS> previous_keys_{};  ///< Previous frame key state

    // mouse button state (GLFW_MOUSE_BUTTON_LAST is 7 as of GLFW 3.4)
    static constexpr int NUM_BUTTONS = 8;
    std::array<bool, NUM_BUTTONS> current_buttons_{};   ///< Current frame button state
    std::array<bool, NUM_BUTTONS> previous_buttons_{};  ///< Previous frame button state

    // mouse position and delta
    vec2 current_mouse_pos_{0.0f, 0.0f};   ///< Current mouse position
    vec2 previous_mouse_pos_{0.0f, 0.0f};  ///< Previous mouse position
    vec2 mouse_delta_{0.0f, 0.0f};         ///< Mouse delta (current - previous)

    // mouse scroll
    vec2 scroll_delta_{0.0f, 0.0f};  ///< Scroll delta (accumulated over frame)
};

}  // namespace luma
