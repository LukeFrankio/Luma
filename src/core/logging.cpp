/**
 * @file logging.cpp
 * @brief Implementation of thread-safe logging system
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/core/logging.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>

namespace luma {

// ============================================================================
// ANSI Color Codes (for colored console output)
// ============================================================================

namespace ansi {
constexpr auto RESET = "\033[0m";
constexpr auto BOLD = "\033[1m";

constexpr auto BLACK = "\033[30m";
constexpr auto RED = "\033[31m";
constexpr auto GREEN = "\033[32m";
constexpr auto YELLOW = "\033[33m";
constexpr auto BLUE = "\033[34m";
constexpr auto MAGENTA = "\033[35m";
constexpr auto CYAN = "\033[36m";
constexpr auto WHITE = "\033[37m";

constexpr auto BRIGHT_BLACK = "\033[90m";
constexpr auto BRIGHT_RED = "\033[91m";
constexpr auto BRIGHT_GREEN = "\033[92m";
constexpr auto BRIGHT_YELLOW = "\033[93m";
constexpr auto BRIGHT_BLUE = "\033[94m";
constexpr auto BRIGHT_MAGENTA = "\033[95m";
constexpr auto BRIGHT_CYAN = "\033[96m";
constexpr auto BRIGHT_WHITE = "\033[97m";
} // namespace ansi

// ============================================================================
// Logger Implementation (pimpl idiom for encapsulation)
// ============================================================================

struct Logger::Impl {
    std::mutex mutex;                          ///< Mutex for thread safety
    std::ofstream log_file;                    ///< Log file stream
    LogLevel min_level = LogLevel::INFO;       ///< Minimum log level
    bool initialized = false;                  ///< Initialization flag
    bool enable_colors = true;                 ///< Color output flag
    
    /**
     * @brief Gets color code for log level
     */
    [[nodiscard]] auto get_level_color(LogLevel level) const noexcept -> const char* {
        if (!enable_colors) {
            return "";
        }
        
        switch (level) {
            case LogLevel::TRACE: return ansi::BRIGHT_BLACK;
            case LogLevel::DEBUG: return ansi::BRIGHT_CYAN;
            case LogLevel::INFO:  return ansi::BRIGHT_GREEN;
            case LogLevel::WARN:  return ansi::BRIGHT_YELLOW;
            case LogLevel::ERROR: return ansi::BRIGHT_RED;
            case LogLevel::FATAL: return std::format("{}{}",ansi::BOLD, ansi::BRIGHT_RED).c_str();
        }
        return "";
    }
    
    /**
     * @brief Formats timestamp as string
     */
    [[nodiscard]] auto format_timestamp() const -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm_buf;
        
        #ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
        #else
        localtime_r(&time_t_now, &tm_buf);
        #endif
        
        return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
                          tm_buf.tm_year + 1900,
                          tm_buf.tm_mon + 1,
                          tm_buf.tm_mday,
                          tm_buf.tm_hour,
                          tm_buf.tm_min,
                          tm_buf.tm_sec,
                          static_cast<int>(ms.count()));
    }
    
    /**
     * @brief Writes log message to console and file
     */
    auto write_log(LogLevel level,
                   const std::source_location& location,
                   std::string_view message) -> void {
        std::lock_guard lock(mutex);
        
        // Skip if below minimum level
        if (level < min_level) {
            return;
        }
        
        // Format log line
        const auto timestamp = format_timestamp();
        const auto thread_id = std::this_thread::get_id();
        const auto level_str = to_string(level);
        const auto color = get_level_color(level);
        
        // Console output (with colors)
        const auto console_line = std::format(
            "{}[{}] [{}] [Thread {}] {}: {}{}\n",
            color,
            timestamp,
            level_str,
            thread_id,
            location.function_name(),
            message,
            enable_colors ? ansi::RESET : ""
        );
        
        // File output (without colors)
        const auto file_line = std::format(
            "[{}] [{}] [Thread {}] {} ({}:{}): {}\n",
            timestamp,
            level_str,
            thread_id,
            location.function_name(),
            location.file_name(),
            location.line(),
            message
        );
        
        // Write to console
        if (level >= LogLevel::ERROR) {
            std::cerr << console_line << std::flush;
        } else {
            std::cout << console_line << std::flush;
        }
        
        // Write to file
        if (log_file.is_open()) {
            log_file << file_line << std::flush;
        }
    }
};

// ============================================================================
// Logger Public Methods
// ============================================================================

auto Logger::instance() -> Logger& {
    static Logger instance;
    return instance;
}

auto Logger::initialize(std::string_view log_file_path) -> Result<void> {
    if (!impl_) {
        impl_ = new Impl();
    }
    
    std::lock_guard lock(impl_->mutex);
    
    if (impl_->initialized) {
        return {};  // Already initialized, return success
    }
    
    // Create logs directory if it doesn't exist
    const auto log_path = std::filesystem::path(log_file_path);
    const auto log_dir = log_path.parent_path();
    
    if (!log_dir.empty() && !std::filesystem::exists(log_dir)) {
        std::error_code ec;
        if (!std::filesystem::create_directories(log_dir, ec)) {
            return std::unexpected(Error{
                ErrorCode::CORE_FILE_IO_ERROR,
                std::format("Failed to create log directory: {}", ec.message())
            });
        }
    }
    
    // Open log file
    impl_->log_file.open(log_path, std::ios::out | std::ios::app);
    if (!impl_->log_file.is_open()) {
        return std::unexpected(Error{
            ErrorCode::CORE_FILE_IO_ERROR,
            std::format("Failed to open log file: {}", log_file_path)
        });
    }
    
    impl_->initialized = true;
    
    // Log initialization message
    impl_->write_log(
        LogLevel::INFO,
        std::source_location::current(),
        std::format("Logger initialized (log file: {})", log_file_path)
    );
    
    return {};  // Success
}

auto Logger::shutdown() -> void {
    if (!impl_) {
        return;
    }
    
    std::lock_guard lock(impl_->mutex);
    
    if (impl_->log_file.is_open()) {
        impl_->write_log(
            LogLevel::INFO,
            std::source_location::current(),
            "Logger shutting down"
        );
        impl_->log_file.close();
    }
    
    impl_->initialized = false;
    
    delete impl_;
    impl_ = nullptr;
}

auto Logger::set_level(LogLevel level) -> void {
    if (!impl_) {
        impl_ = new Impl();
    }
    
    std::lock_guard lock(impl_->mutex);
    impl_->min_level = level;
}

auto Logger::get_level() const -> LogLevel {
    if (!impl_) {
        return LogLevel::INFO;
    }
    
    std::lock_guard lock(impl_->mutex);
    return impl_->min_level;
}

auto Logger::log_impl(LogLevel level, const std::source_location& location, 
                      std::string_view message) -> void {
    if (!impl_) {
        impl_ = new Impl();
    }
    impl_->write_log(level, location, message);
}

} // namespace luma
