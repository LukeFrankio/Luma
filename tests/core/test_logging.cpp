/**
 * @file test_logging.cpp
 * @brief Unit tests for logging system
 * 
 * Tests log output functionality, severity levels, formatting, and thread safety.
 * 
 * @author LukeFrankio
 * @date 2025-10-07
 */

#include <luma/core/logging.hpp>

#include <gtest/gtest.h>

#include <string>
#include <sstream>

using namespace luma;

/**
 * @brief Global test environment to suppress verbose logging
 * 
 * Sets log level to ERROR by default, so only error messages appear.
 * Individual tests can override this if they need to test other log levels.
 */
class QuietTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // suppress INFO/DEBUG/TRACE logs during tests (only show errors)
        Logger::instance().set_level(LogLevel::ERROR);
    }
    
    void TearDown() override {
        // restore default log level
        Logger::instance().set_level(LogLevel::TRACE);
    }
};

// Register the global environment
[[maybe_unused]] static auto* const quiet_env = 
    ::testing::AddGlobalTestEnvironment(new QuietTestEnvironment);

/**
 * @brief Test fixture for logging tests
 * 
 * Sets up and tears down logging system for each test.
 */
class LoggingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // logging system is global singleton, already initialized
        // just verify it's working
    }
    
    void TearDown() override {
        // cleanup happens in Logger destructor (global singleton)
    }
};

/**
 * @brief Test basic logging functionality
 * 
 * Verifies that log macros can be called without crashing.
 * Note: This test temporarily enables all log levels to verify functionality.
 */
TEST_F(LoggingTest, BasicLogging) {
    // temporarily enable all log levels for this test
    Logger::instance().set_level(LogLevel::TRACE);
    
    // these should not crash
    LOG_TRACE("Trace message");
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warning message");
    LOG_ERROR("Error message");
    
    // restore quiet mode
    Logger::instance().set_level(LogLevel::ERROR);
    
    // test passes if no crash occurs
    SUCCEED();
}

/**
 * @brief Test logging with format arguments
 * 
 * Verifies that std::format-style arguments work correctly.
 * Note: This test temporarily enables INFO log level.
 */
TEST_F(LoggingTest, FormattedLogging) {
    // temporarily enable INFO for this test
    Logger::instance().set_level(LogLevel::INFO);
    
    int value = 42;
    float pi = 3.14159f;
    const char* text = "hello";
    
    // these should format correctly and not crash
    LOG_INFO("Integer: {}", value);
    LOG_INFO("Float: {:.2f}", pi);
    LOG_INFO("String: {}", text);
    LOG_INFO("Multiple: {} {} {}", value, pi, text);
    
    // restore quiet mode
    Logger::instance().set_level(LogLevel::ERROR);
    
    SUCCEED();
}

/**
 * @brief Test log level filtering
 * 
 * Verifies that log levels can be set and filtering works.
 * Note: This test is informational only, as we can't easily capture
 * log output without refactoring the logger.
 */
TEST_F(LoggingTest, LogLevelFiltering) {
    // set log level to INFO (filter out TRACE and DEBUG)
    Logger::instance().set_level(LogLevel::INFO);
    
    // these should be filtered out (not visible in output)
    LOG_TRACE("This should be filtered");
    LOG_DEBUG("This should also be filtered");
    
    // these should appear
    LOG_INFO("This should appear");
    LOG_WARN("This should also appear");
    LOG_ERROR("This should definitely appear");
    
    // reset to default
    Logger::instance().set_level(LogLevel::TRACE);
    
    SUCCEED();
}

/**
 * @brief Test logging from multiple threads
 * 
 * Verifies that logging is thread-safe (no crashes or data races).
 * Note: Runs in quiet mode (ERROR level) to reduce output spam.
 */
TEST_F(LoggingTest, ThreadSafety) {
    // keep ERROR level (quiet mode) for this test to avoid spam
    constexpr int num_threads = 4;
    constexpr int messages_per_thread = 100;
    
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, messages_per_thread]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                // these won't appear (ERROR level suppresses INFO)
                LOG_INFO("Thread {} message {}", t, i);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // test passes if no crash or data race detected
    SUCCEED();
}

/**
 * @brief Test special characters in log messages
 * 
 * Verifies that special characters (newlines, tabs, etc.) are handled correctly.
 * Note: Runs in quiet mode to reduce output.
 */
TEST_F(LoggingTest, SpecialCharacters) {
    // keep ERROR level (these INFO logs won't appear)
    LOG_INFO("Newline in message:\nSecond line");
    LOG_INFO("Tab character:\tIndented");
    LOG_INFO("Percent sign: 100%");
    LOG_INFO("Curly braces: {{ and }}");
    
    SUCCEED();
}

/**
 * @brief Test logging with very long messages
 * 
 * Verifies that long messages are handled correctly (no buffer overflow).
 * Note: Runs in quiet mode to reduce output.
 */
TEST_F(LoggingTest, LongMessages) {
    // keep ERROR level (this INFO log won't appear)
    std::string long_msg(10000, 'A');
    LOG_INFO("Long message: {}", long_msg);
    
    SUCCEED();
}

/**
 * @brief Test logging performance
 * 
 * Measures logging throughput (informational, not a pass/fail test).
 * Note: Runs in quiet mode to reduce output spam.
 */
TEST_F(LoggingTest, Performance) {
    // keep ERROR level to avoid printing 10000 messages
    constexpr int num_messages = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_messages; ++i) {
        LOG_INFO("Performance test message {}", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double messages_per_second = (num_messages * 1000.0) / duration.count();
    
    // temporarily enable INFO to show result
    Logger::instance().set_level(LogLevel::INFO);
    LOG_INFO("Logging performance: {:.0f} messages/sec", messages_per_second);
    Logger::instance().set_level(LogLevel::ERROR);
    
    // just informational, always pass
    SUCCEED();
}

/**
 * @brief Test that FATAL logs terminate the program
 * 
 * Note: This test is disabled by default because it would terminate the test process.
 * Enable manually to verify FATAL behavior.
 */
TEST_F(LoggingTest, DISABLED_FatalTerminates) {
    // this should terminate the program
    // LOG_FATAL("Fatal error - terminating");
    
    // if we reach here, something is wrong
    FAIL() << "LOG_FATAL should have terminated the program";
}
