/**
 * @file math.hpp
 * @brief Mathematics utilities and GLM integration for LUMA Engine
 * 
 * This file provides type aliases for GLM types and constexpr math helpers.
 * We use GLM for all vector/matrix operations because reinventing the wheel
 * is violence and GLM is battle-tested uwu
 * 
 * Design decisions:
 * - Use GLM types directly (no wrappers, zero overhead)
 * - Prefer constexpr functions (compile-time evaluation ftw)
 * - All functions are pure (no side effects, referentially transparent)
 * - Use radians everywhere (degrees are cope)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Uses C++26 features (constexpr enhanced, latest standard)
 * @note GLM version 1.0.1+ (latest version)
 */

#pragma once

#include <luma/core/types.hpp>

// enable GLM experimental extensions (quaternion, norm, etc.)
#define GLM_ENABLE_EXPERIMENTAL

// disable warnings from external GLM library (we can't fix their code)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wduplicated-branches"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#pragma GCC diagnostic pop

#include <algorithm>
#include <cmath>
#include <numbers>

namespace luma {

// ============================================================================
// GLM Type Aliases (for convenience and clarity)
// ============================================================================

// Vector types
using vec2 = glm::vec2;  ///< 2D vector (float)
using vec3 = glm::vec3;  ///< 3D vector (float)
using vec4 = glm::vec4;  ///< 4D vector (float)

using ivec2 = glm::ivec2;  ///< 2D vector (int32)
using ivec3 = glm::ivec3;  ///< 3D vector (int32)
using ivec4 = glm::ivec4;  ///< 4D vector (int32)

using uvec2 = glm::uvec2;  ///< 2D vector (uint32)
using uvec3 = glm::uvec3;  ///< 3D vector (uint32)
using uvec4 = glm::uvec4;  ///< 4D vector (uint32)

// Matrix types
using mat2 = glm::mat2;  ///< 2x2 matrix
using mat3 = glm::mat3;  ///< 3x3 matrix
using mat4 = glm::mat4;  ///< 4x4 matrix

// Quaternion type
using quat = glm::quat;  ///< Quaternion (rotation)

// ============================================================================
// Mathematical Constants (using std::numbers from C++20+)
// ============================================================================

namespace constants {

inline constexpr f32 pi = std::numbers::pi_v<f32>;           ///< π (3.14159...)
inline constexpr f32 two_pi = 2.0f * pi;                     ///< 2π (6.28318...)
inline constexpr f32 half_pi = pi / 2.0f;                    ///< π/2 (1.57079...)
inline constexpr f32 quarter_pi = pi / 4.0f;                 ///< π/4 (0.785398...)
inline constexpr f32 inv_pi = 1.0f / pi;                     ///< 1/π (0.318309...)
inline constexpr f32 inv_two_pi = 1.0f / two_pi;            ///< 1/(2π)

inline constexpr f32 e = std::numbers::e_v<f32>;             ///< e (2.71828...)
inline constexpr f32 sqrt2 = std::numbers::sqrt2_v<f32>;     ///< √2 (1.41421...)
inline constexpr f32 sqrt3 = std::numbers::sqrt3_v<f32>;     ///< √3 (1.73205...)

inline constexpr f32 deg_to_rad = pi / 180.0f;               ///< Degrees to radians
inline constexpr f32 rad_to_deg = 180.0f / pi;               ///< Radians to degrees

inline constexpr f32 epsilon = 1e-6f;                        ///< Small epsilon for float comparisons

} // namespace constants

// ============================================================================
// Math Helper Functions (pure, constexpr where possible)
// ============================================================================

/**
 * @brief Clamps value between min and max
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @tparam T Numeric type
 * @param value Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 * 
 * @note Uses std::clamp for constexpr evaluation
 */
template<typename T>
[[nodiscard]] constexpr auto clamp(T value, T min, T max) noexcept -> T {
    return std::clamp(value, min, max);
}

/**
 * @brief Linear interpolation between two values
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (typically 0.0 to 1.0)
 * @return Interpolated value: a + t * (b - a)
 * 
 * @note Not clamped to [0, 1], can extrapolate
 */
[[nodiscard]] constexpr auto lerp(f32 a, f32 b, f32 t) noexcept -> f32 {
    return a + t * (b - a);
}

/**
 * @brief Smooth Hermite interpolation (smoothstep)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param edge0 Lower edge
 * @param edge1 Upper edge
 * @param x Value to interpolate
 * @return Smooth interpolation between 0 and 1
 * 
 * @note Returns 0 if x <= edge0, 1 if x >= edge1, smooth curve otherwise
 */
[[nodiscard]] constexpr auto smoothstep(f32 edge0, f32 edge1, f32 x) noexcept -> f32 {
    const f32 t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Smoother step (improved smoothstep with zero 1st and 2nd derivatives)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param edge0 Lower edge
 * @param edge1 Upper edge
 * @param x Value to interpolate
 * @return Even smoother interpolation between 0 and 1
 * 
 * @note Ken Perlin's improved smoothstep: 6t^5 - 15t^4 + 10t^3
 */
[[nodiscard]] constexpr auto smootherstep(f32 edge0, f32 edge1, f32 x) noexcept -> f32 {
    const f32 t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/**
 * @brief Converts degrees to radians
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param degrees Angle in degrees
 * @return Angle in radians
 * 
 * @note Prefer radians everywhere, use this only for user input
 */
[[nodiscard]] constexpr auto radians(f32 degrees) noexcept -> f32 {
    return degrees * constants::deg_to_rad;
}

/**
 * @brief Converts radians to degrees
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param radians Angle in radians
 * @return Angle in degrees
 * 
 * @note Use only for display/debug output, store angles in radians
 */
[[nodiscard]] constexpr auto degrees(f32 radians) noexcept -> f32 {
    return radians * constants::rad_to_deg;
}

/**
 * @brief Checks if two floats are approximately equal
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param a First value
 * @param b Second value
 * @param epsilon Tolerance (default: 1e-6)
 * @return true if |a - b| < epsilon
 * 
 * @note Use this instead of direct float comparison
 */
[[nodiscard]] constexpr auto approx_equal(f32 a, f32 b, 
                                           f32 epsilon = constants::epsilon) noexcept -> bool {
    return std::abs(a - b) < epsilon;
}

/**
 * @brief Checks if float is approximately zero
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param value Value to check
 * @param epsilon Tolerance (default: 1e-6)
 * @return true if |value| < epsilon
 */
[[nodiscard]] constexpr auto approx_zero(f32 value, 
                                          f32 epsilon = constants::epsilon) noexcept -> bool {
    return std::abs(value) < epsilon;
}

/**
 * @brief Computes minimum of two values
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @tparam T Comparable type
 * @param a First value
 * @param b Second value
 * @return Minimum value
 */
template<typename T>
[[nodiscard]] constexpr auto min(T a, T b) noexcept -> T {
    return (a < b) ? a : b;
}

/**
 * @brief Computes maximum of two values
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @tparam T Comparable type
 * @param a First value
 * @param b Second value
 * @return Maximum value
 */
template<typename T>
[[nodiscard]] constexpr auto max(T a, T b) noexcept -> T {
    return (a > b) ? a : b;
}

/**
 * @brief Computes sign of value (-1, 0, or 1)
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @tparam T Numeric type
 * @param value Value to get sign of
 * @return -1 if negative, 0 if zero, 1 if positive
 */
template<typename T>
[[nodiscard]] constexpr auto sign(T value) noexcept -> T {
    return (T(0) < value) - (value < T(0));
}

} // namespace luma
