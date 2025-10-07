/**
 * @file time.cpp
 * @brief Implementation of time measurement utilities
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/core/time.hpp>

namespace luma {

// ============================================================================
// Timer Implementation
// ============================================================================

Timer::Timer() noexcept 
    : start_time_(Clock::now())
    , last_tick_time_(start_time_) {}

auto Timer::reset() noexcept -> void {
    start_time_ = Clock::now();
    last_tick_time_ = start_time_;
    delta_time_ = 0.0f;
}

auto Timer::tick() noexcept -> f32 {
    const auto current_time = Clock::now();
    const auto delta_duration = current_time - last_tick_time_;
    
    delta_time_ = std::chrono::duration<f32>(delta_duration).count();
    last_tick_time_ = current_time;
    
    return delta_time_;
}

auto Timer::elapsed() const noexcept -> f32 {
    const auto current_time = Clock::now();
    const auto elapsed_duration = current_time - start_time_;
    return std::chrono::duration<f32>(elapsed_duration).count();
}

// ============================================================================
// FPSCounter Implementation
// ============================================================================

FPSCounter::FPSCounter(u32 window_size) noexcept
    : window_size_(window_size) {}

auto FPSCounter::update(f32 delta_time) noexcept -> void {
    time_accumulator_ += delta_time;
    frame_count_++;
    
    // Update FPS every N frames
    if (frame_count_ >= window_size_) {
        current_fps_ = static_cast<f32>(frame_count_) / time_accumulator_;
        frame_count_ = 0;
        time_accumulator_ = 0.0f;
    }
}

auto FPSCounter::reset() noexcept -> void {
    frame_count_ = 0;
    time_accumulator_ = 0.0f;
    current_fps_ = 0.0f;
}

} // namespace luma
