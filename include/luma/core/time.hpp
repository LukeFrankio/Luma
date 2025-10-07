/**
 * @file time.hpp
 * @brief Time measurement and FPS counting for LUMA Engine
 * 
 * This file provides utilities for measuring elapsed time, delta time between
 * frames, and FPS counting. Uses std::chrono for high-resolution timing uwu
 * 
 * Design decisions:
 * - Use std::chrono for cross-platform timing
 * - High-resolution clock for accurate measurements
 * - Immutable timer state (functional where possible)
 * - Zero-overhead abstractions
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Uses C++26 features (latest standard)
 * @note Thread-safe when each thread has its own Timer instance
 */

#pragma once

#include <luma/core/types.hpp>

#include <chrono>

namespace luma {

// ============================================================================
// Timer Class (high-resolution timing)
// ============================================================================

/**
 * @class Timer
 * @brief High-resolution timer for measuring elapsed time
 * 
 * Provides delta time calculation and elapsed time measurement using
 * std::chrono::steady_clock for monotonic time.
 * 
 * ✨ MOSTLY PURE (only modifies internal state)
 * 
 * @note Not thread-safe: each thread should have its own Timer instance
 */
class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    
    /**
     * @brief Constructs timer and starts counting
     * 
     * ⚠️ IMPURE (queries system clock)
     */
    Timer() noexcept;
    
    /**
     * @brief Resets timer to current time
     * 
     * ⚠️ IMPURE (queries system clock)
     */
    auto reset() noexcept -> void;
    
    /**
     * @brief Updates delta time and returns time since last tick
     * 
     * ⚠️ IMPURE (queries system clock, modifies state)
     * 
     * @return Delta time in seconds since last tick
     * 
     * @note Call once per frame for frame delta time
     */
    auto tick() noexcept -> f32;
    
    /**
     * @brief Gets elapsed time since timer creation/reset
     * 
     * ⚠️ IMPURE (queries system clock)
     * 
     * @return Elapsed time in seconds
     */
    [[nodiscard]] auto elapsed() const noexcept -> f32;
    
    /**
     * @brief Gets delta time from last tick
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Delta time in seconds from last tick
     */
    [[nodiscard]] auto delta_time() const noexcept -> f32 {
        return delta_time_;
    }
    
private:
    TimePoint start_time_;      ///< Timer start time
    TimePoint last_tick_time_;  ///< Last tick time
    f32 delta_time_ = 0.0f;     ///< Delta time in seconds
};

// ============================================================================
// FPS Counter (tracks frames per second)
// ============================================================================

/**
 * @class FPSCounter
 * @brief Frames-per-second counter with moving average
 * 
 * Tracks FPS over a window of frames for smooth, stable FPS reporting.
 * 
 * ⚠️ IMPURE (modifies internal state)
 * 
 * @note Not thread-safe: use one instance per rendering thread
 */
class FPSCounter {
public:
    /**
     * @brief Constructs FPS counter
     * 
     * @param window_size Number of frames to average over (default: 60)
     */
    explicit FPSCounter(u32 window_size = 60) noexcept;
    
    /**
     * @brief Updates FPS counter with delta time
     * 
     * ⚠️ IMPURE (modifies state)
     * 
     * @param delta_time Delta time in seconds since last frame
     */
    auto update(f32 delta_time) noexcept -> void;
    
    /**
     * @brief Gets current FPS (averaged over window)
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Current frames per second
     */
    [[nodiscard]] auto get_fps() const noexcept -> f32 {
        return current_fps_;
    }
    
    /**
     * @brief Gets frame time in milliseconds
     * 
     * ✨ PURE FUNCTION ✨ (read-only access)
     * 
     * @return Frame time in milliseconds
     */
    [[nodiscard]] auto get_frame_time_ms() const noexcept -> f32 {
        return (current_fps_ > 0.0f) ? (1000.0f / current_fps_) : 0.0f;
    }
    
    /**
     * @brief Resets FPS counter
     * 
     * ⚠️ IMPURE (modifies state)
     */
    auto reset() noexcept -> void;
    
private:
    u32 window_size_;           ///< Number of frames to average
    u32 frame_count_ = 0;       ///< Current frame count
    f32 time_accumulator_ = 0.0f;  ///< Accumulated time
    f32 current_fps_ = 0.0f;    ///< Current FPS
};

} // namespace luma
