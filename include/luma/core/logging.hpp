/**
 * @file logging.hpp
 * @brief Thread-safe logging system for LUMA Engine
 * 
 * This file provides a comprehensive logging system with multiple severity
 * levels, colored console output, file output, and thread-safe operation.
 * Uses std::format for type-safe formatted output (C++20+ feature) uwu
 * 
 * Design decisions:
 * - Thread-safe logging with mutex (no data races)
 * - Colored console output (ANSI escape codes)
 * - File output with rotation (logs/luma.log)
 * - Compile-time log level filtering (zero overhead for disabled logs)
 * - Format string validation at compile time (std::format)
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 * @version 1.0
 * 
 * @note Uses C++26 features (std::format enhanced, latest standard)
 * @note Thread-safe implementation (mutex-protected global state)
 */

#pragma once

#include <luma/core/types.hpp>

#include <format>
#include <source_location>
#include <string_view>

namespace luma {

// ============================================================================
// Log Level Enumeration
// ============================================================================

/**
 * @enum LogLevel
 * @brief Severity levels for log messages
 * 
 * Ordered from least to most severe. Messages are only logged if their
 * level is >= the current minimum log level.
 */
enum class LogLevel : u8 {
    TRACE = 0,  ///< Trace: very detailed debug information
    DEBUG = 1,  ///< Debug: general debug information
    INFO = 2,   ///< Info: informational messages
    WARN = 3,   ///< Warning: something unexpected but recoverable
    ERROR = 4,  ///< Error: operation failed but program continues
    FATAL = 5   ///< Fatal: critical error, program should terminate
};

/**
 * @brief Converts log level to string
 * 
 * ✨ PURE FUNCTION ✨
 * 
 * @param level Log level
 * @return String representation of level
 */
[[nodiscard]] constexpr auto to_string(LogLevel level) noexcept -> std::string_view {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "UNKNOWN";
}

// ============================================================================
// Logger Class (singleton with thread-safe access)
// ============================================================================

/**
 * @class Logger
 * @brief Thread-safe logging system
 * 
 * Provides logging to console and file with colored output, timestamps,
 * thread IDs, and source location information.
 * 
 * ⚠️ IMPURE CLASS (has side effects)
 * 
 * Side effects:
 * - Writes to console (stdout/stderr)
 * - Writes to log file (logs/luma.log)
 * - Modifies global state (log level, file handle)
 * 
 * @note Singleton pattern used for global access (not ideal but pragmatic)
 * @note Thread-safe: all public methods use mutex
 */
class Logger {
public:
    /**
     * @brief Gets singleton instance
     * 
     * ⚠️ IMPURE FUNCTION (returns reference to global state)
     * 
     * @return Reference to logger instance
     */
    static auto instance() -> Logger&;
    
    /**
     * @brief Initializes logger (opens log file)
     * 
     * ⚠️ IMPURE FUNCTION (file I/O)
     * 
     * @param log_file_path Path to log file (default: "logs/luma.log")
     * @return Result indicating success or error
     */
    auto initialize(std::string_view log_file_path = "logs/luma.log") -> Result<void>;
    
    /**
     * @brief Shuts down logger (closes log file)
     * 
     * ⚠️ IMPURE FUNCTION (file I/O)
     */
    auto shutdown() -> void;
    
    /**
     * @brief Sets minimum log level
     * 
     * ⚠️ IMPURE FUNCTION (modifies global state)
     * 
     * @param level Minimum log level (messages below this are ignored)
     */
    auto set_level(LogLevel level) -> void;
    
    /**
     * @brief Gets current minimum log level
     * 
     * ✨ PURE FUNCTION ✨ (read-only access to state)
     * 
     * @return Current log level
     */
    [[nodiscard]] auto get_level() const -> LogLevel;
    
    /**
     * @brief Logs a message with format string
     * 
     * ⚠️ IMPURE FUNCTION (I/O operations)
     * 
     * @tparam Args Format argument types
     * @param level Log level
     * @param location Source location (automatic via std::source_location)
     * @param format Format string (std::format compatible)
     * @param args Format arguments
     */
    template<typename... Args>
    auto log(LogLevel level, 
             const std::source_location& location,
             std::format_string<Args...> format,
             Args&&... args) -> void {
        // Format message
        const auto message = std::format(format, std::forward<Args>(args)...);
        
        // Write log (implemented in .cpp, handles impl creation)
        log_impl(level, location, message);
    }
    
    // Delete copy and move (singleton)
    Logger(const Logger&) = delete;
    auto operator=(const Logger&) -> Logger& = delete;
    Logger(Logger&&) = delete;
    auto operator=(Logger&&) -> Logger& = delete;
    
private:
    Logger() = default;
    ~Logger() = default;
    
    // Helper for template implementation
    auto log_impl(LogLevel level, const std::source_location& location, std::string_view message) -> void;
    
    struct Impl;
    Impl* impl_ = nullptr;  ///< Pointer to implementation (pimpl idiom)
};

} // namespace luma

// ============================================================================
// Logging Macros (convenient wrappers with source location)
// ============================================================================

/**
 * @def LOG_TRACE
 * @brief Logs trace message
 * 
 * Usage: LOG_TRACE("value = {}", value);
 */
#define LOG_TRACE(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::TRACE, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)

/**
 * @def LOG_DEBUG
 * @brief Logs debug message
 * 
 * Usage: LOG_DEBUG("Initialized {} with {} items", name, count);
 */
#define LOG_DEBUG(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::DEBUG, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)

/**
 * @def LOG_INFO
 * @brief Logs info message
 * 
 * Usage: LOG_INFO("Application started");
 */
#define LOG_INFO(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::INFO, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)

/**
 * @def LOG_WARN
 * @brief Logs warning message
 * 
 * Usage: LOG_WARN("Performance degraded: {} FPS", fps);
 */
#define LOG_WARN(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::WARN, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)

/**
 * @def LOG_ERROR
 * @brief Logs error message
 * 
 * Usage: LOG_ERROR("Failed to load file: {}", filename);
 */
#define LOG_ERROR(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::ERROR, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)

/**
 * @def LOG_FATAL
 * @brief Logs fatal error message
 * 
 * Usage: LOG_FATAL("Out of memory! Allocation failed");
 */
#define LOG_FATAL(...) \
    ::luma::Logger::instance().log(::luma::LogLevel::FATAL, \
                                    std::source_location::current(), \
                                    __VA_ARGS__)
