#pragma once

// NOTE: This is a simple example logger for demonstration purposes.
// For production use, consider using a proper logging library like spdlog or slick_logger.

#include <iostream>
#include <chrono>
#include <format>

namespace {
    // Single function using vformat handles both cases: with and without arguments
    template<typename... Args>
    void log_impl(const std::string& level, const std::string& fmt, Args&&... args)
    {
        std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now())
                  << "[" << level << "] " << std::vformat(fmt, std::make_format_args(args...)) << std::endl;
    }
}

// Works with any number of arguments
#define LOG_DEBUG(fmt, ...) log_impl("DEBUG", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_impl("INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) log_impl("WARNING", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_impl("ERROR", fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) log_impl("TRACE", fmt, ##__VA_ARGS__)
