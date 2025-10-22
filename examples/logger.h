#pragma once

#include <iostream>
#include <chrono>
#include <format>

#define LOG_DEBUG(fmt, ...) std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[DEBUG] " << std::format(fmt __VA_OPT__(,) __VA_ARGS__) << std::endl
#define LOG_INFO(fmt, ...) std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[INFO] " << std::format(fmt __VA_OPT__(,) __VA_ARGS__) << std::endl
#define LOG_WARN(fmt, ...) std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[WARNING] " << std::format(fmt __VA_OPT__(,) __VA_ARGS__) << std::endl
#define LOG_ERROR(fmt, ...) std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[ERROR] " << std::format(fmt __VA_OPT__(,) __VA_ARGS__) << std::endl
#define LOG_TRACE(fmt, ...) std::cout << std::format("{:%Y-%m-%d %H:%M:%S} ", std::chrono::system_clock::now()) << "[TRACE] " << std::format(fmt __VA_OPT__(,) __VA_ARGS__) << std::endl
