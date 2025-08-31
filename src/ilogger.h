#pragma once

#include <string>
#include <format>

namespace slick_socket
{

struct ILogger
{
    virtual ~ILogger() = default;
    virtual void logTrace(const std::string& format, std::format_args args = std::make_format_args()) = 0;
    virtual void logDebug(const std::string& format, std::format_args args = std::make_format_args()) = 0;
    virtual void logInfo(const std::string& format, std::format_args args = std::make_format_args()) = 0;
    virtual void logWarning(const std::string& format, std::format_args args = std::make_format_args()) = 0;
    virtual void logError(const std::string& format, std::format_args args = std::make_format_args()) = 0;
};

struct NullLogger final : public ILogger
{
    static NullLogger& instance()
    {
        static NullLogger instance;
        return instance;
    }

    void logTrace(const std::string& format, std::format_args args = std::make_format_args()) override {}
    void logDebug(const std::string& format, std::format_args args = std::make_format_args()) override {}
    void logInfo(const std::string& format, std::format_args args = std::make_format_args()) override {}
    void logWarning(const std::string& format, std::format_args args = std::make_format_args()) override {}
    void logError(const std::string& format, std::format_args args = std::make_format_args()) override {}

private:
    NullLogger() = default;
    ~NullLogger() = default;
    NullLogger(const NullLogger&) = delete;
    NullLogger& operator=(const NullLogger&) = delete;
    NullLogger(NullLogger&&) = delete;
    NullLogger& operator=(NullLogger&&) = delete;
};

} // namespace slick_socket