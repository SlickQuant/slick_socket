#pragma once

#include <string>
#include <iostream>
#include <format>
#include <chrono>

namespace slick_socket
{

struct ConsoleLogger final
{
    static ConsoleLogger& instance()
    {
        static ConsoleLogger instance;
        return instance;
    }

    void logTrace(const std::string& format, auto&&... args)
    {
        std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[TRACE] "
            << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }
    void logDebug(const std::string& format, auto&&... args)
    {
        std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[DEBUG] "
            << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }
    void logInfo(const std::string& format, auto&&... args)
    {
        std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[INFO] "
            << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }
    void logWarning(const std::string& format, auto&&... args)
    {
        std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[WARN] "
            << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }
    void logError(const std::string& format, auto&&... args)
    {
        std::cerr << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[ERROR] "
            << std::vformat(format, std::make_format_args(args...)) << std::endl;
    }

private:
    ConsoleLogger() = default;
    ~ConsoleLogger() = default;
    ConsoleLogger(const ConsoleLogger&) = delete;
    ConsoleLogger& operator=(const ConsoleLogger&) = delete;
    ConsoleLogger(ConsoleLogger&&) = delete;
    ConsoleLogger& operator=(ConsoleLogger&&) = delete;
};

} // namespace slick_socket
