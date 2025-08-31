#include <iostream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include "src/tcp_server.h"

// Custom logger function that demonstrates handling format strings
// In practice, you would use a proper formatting library like fmt, Boost.Format, etc.
void my_logger(slick_socket::LogLevel level, const std::string& format, ...) {
    // Get the current time for timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Format: [TIMESTAMP] [LEVEL] formatted_message
    std::cout << std::ctime(&time_t_now); // This includes newline

    // Print log level
    switch(level) {
        case slick_socket::LogLevel::TRACE:
            std::cout << "[TRACE] ";
            break;
        case slick_socket::LogLevel::DEBUG:
            std::cout << "[DEBUG] ";
            break;
        case slick_socket::LogLevel::INFO:
            std::cout << "[INFO] ";
            break;
        case slick_socket::LogLevel::WARNING:
            std::cout << "[WARNING] ";
            break;
        case slick_socket::LogLevel::ERROR:
            std::cout << "[ERROR] ";
            break;
    }

    // Simple demonstration of handling format + args
    // In real code, you would use your preferred formatting library
    std::cout << "Format: '" << format << "' (args handling would go here)" << std::endl;
}

int main() {
    std::cout << "Testing user-configurable logging with all log levels..." << std::endl;

    // Create server with default config (no-op logger)
    slick_socket::TCPServer::Config config;
    config.port = 8080;

    slick_socket::TCPServer server(config);

    // Set custom logger that receives format strings directly
    server.set_logger(my_logger);

    std::cout << "Starting server..." << std::endl;
    if (server.start()) {
        std::cout << "Server started successfully!" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();
        std::cout << "Server stopped." << std::endl;
    } else {
        std::cout << "Failed to start server." << std::endl;
    }

    return 0;
}
