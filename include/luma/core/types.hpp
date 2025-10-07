/**
 * @file types.hpp
 * @brief Fundamental type definitions for LUMA Engine
 * 
 * This file provides type aliases, error handling types, and core utilities
 * used throughout the LUMA Engine. We use std::expected for error handling
 * (no exceptions!) and prefer explicit types over language primitives uwu
 * 
 * Design decisions:
 * - Use std::expected instead of exceptions (functional error handling)
 * - Explicit sized integer types (no implicit conversions)
 * - Strong type safety (no implicit casts where possible)
 * - Zero-cost abstractions (everything inlines or constexpr)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Uses C++26 features (std::expected, latest standard)
 * @note Compiled with GCC 15+ (compiler supremacy uwu)
 */

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace luma {

// ============================================================================
// Fundamental Types (explicit sized integers for clarity)
// ============================================================================

using u8 = std::uint8_t;    ///< 8-bit unsigned integer
using u16 = std::uint16_t;  ///< 16-bit unsigned integer
using u32 = std::uint32_t;  ///< 32-bit unsigned integer
using u64 = std::uint64_t;  ///< 64-bit unsigned integer

using i8 = std::int8_t;     ///< 8-bit signed integer
using i16 = std::int16_t;   ///< 16-bit signed integer
using i32 = std::int32_t;   ///< 32-bit signed integer
using i64 = std::int64_t;   ///< 64-bit signed integer

using f32 = float;   ///< 32-bit floating point
using f64 = double;  ///< 64-bit floating point

// ============================================================================
// Error Handling (functional error handling, no exceptions!)
// ============================================================================

/**
 * @enum ErrorCode
 * @brief Error codes for LUMA Engine operations
 * 
 * All error codes are prefixed by module:
 * - CORE_* for core module errors
 * - VULKAN_* for Vulkan backend errors
 * - SCENE_* for scene management errors
 * - etc.
 * 
 * @note Using enum class for strong typing (no implicit conversions)
 */
enum class ErrorCode : u32 {
    // Success (not an error)
    OK = 0,
    
    // Core module errors (1000-1999)
    CORE_UNKNOWN = 1000,
    CORE_OUT_OF_MEMORY = 1001,
    CORE_INVALID_ARGUMENT = 1002,
    CORE_FILE_NOT_FOUND = 1003,
    CORE_FILE_IO_ERROR = 1004,
    
    // Vulkan module errors (2000-2999)
    VULKAN_UNKNOWN = 2000,
    VULKAN_INITIALIZATION_FAILED = 2001,
    VULKAN_DEVICE_LOST = 2002,
    VULKAN_OUT_OF_MEMORY = 2003,
    VULKAN_SURFACE_LOST = 2004,
    VULKAN_SWAPCHAIN_OUT_OF_DATE = 2005,
    
    // Scene module errors (3000-3999)
    SCENE_UNKNOWN = 3000,
    SCENE_INVALID_SCENE_FILE = 3001,
    SCENE_MISSING_REQUIRED_FIELD = 3002,
    
    // Asset module errors (4000-4999)
    ASSET_UNKNOWN = 4000,
    ASSET_LOAD_FAILED = 4001,
    ASSET_INVALID_FORMAT = 4002,
    
    // Add more error codes as needed for other modules
};

/**
 * @struct Error
 * @brief Error information with code and message
 * 
 * This struct combines an error code with a human-readable message.
 * Used with std::expected for functional error handling uwu
 * 
 * @note Immutable after construction (functional paradigm)
 */
struct Error {
    ErrorCode code;      ///< Error code
    std::string message; ///< Human-readable error message
    
    /**
     * @brief Constructs an error with code and message
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @param error_code Error code
     * @param error_message Error message
     */
    constexpr Error(ErrorCode error_code, std::string_view error_message)
        : code(error_code), message(error_message) {}
    
    /**
     * @brief Gets error message
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return Error message as string view
     */
    [[nodiscard]] constexpr auto what() const noexcept -> std::string_view {
        return message;
    }
    
    /**
     * @brief Checks if error is OK (not actually an error)
     * 
     * ✨ PURE FUNCTION ✨
     * 
     * @return true if error code is OK
     */
    [[nodiscard]] constexpr auto is_ok() const noexcept -> bool {
        return code == ErrorCode::OK;
    }
};

/**
 * @typedef Result<T>
 * @brief Result type for operations that can fail
 * 
 * This is an alias for std::expected<T, Error>, providing functional
 * error handling without exceptions. Operations return Result<T> to
 * indicate success (T) or failure (Error).
 * 
 * Usage:
 * @code
 * auto result = risky_operation();
 * if (result) {
 *     // success: use *result
 *     do_something(*result);
 * } else {
 *     // failure: check result.error()
 *     LOG_ERROR("Operation failed: {}", result.error().what());
 * }
 * @endcode
 * 
 * @tparam T The type of the success value
 * 
 * @note Prefer monadic operations (and_then, or_else, transform) over
 *       explicit if-else when composing multiple operations
 */
template<typename T>
using Result = std::expected<T, Error>;

// ============================================================================
// Utility Types
// ============================================================================

/**
 * @typedef NonCopyable
 * @brief Base class for non-copyable types
 * 
 * Inherit from this to make a class non-copyable but movable.
 * 
 * @note Prefer composition over inheritance, but this is acceptable
 *       for enforcing non-copyability at compile time
 */
struct NonCopyable {
    NonCopyable() = default;
    ~NonCopyable() = default;
    
    NonCopyable(const NonCopyable&) = delete;
    auto operator=(const NonCopyable&) -> NonCopyable& = delete;
    
    NonCopyable(NonCopyable&&) noexcept = default;
    auto operator=(NonCopyable&&) noexcept -> NonCopyable& = default;
};

/**
 * @typedef NonMovable
 * @brief Base class for non-movable types
 * 
 * Inherit from this to make a class non-copyable and non-movable.
 * 
 * @note Use sparingly, most types should be movable
 */
struct NonMovable {
    NonMovable() = default;
    ~NonMovable() = default;
    
    NonMovable(const NonMovable&) = delete;
    auto operator=(const NonMovable&) -> NonMovable& = delete;
    
    NonMovable(NonMovable&&) = delete;
    auto operator=(NonMovable&&) -> NonMovable& = delete;
};

} // namespace luma
