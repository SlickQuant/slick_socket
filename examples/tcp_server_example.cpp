#include <tcp_server_win32.h>
#include <iostream>
#include <string>
#include <format>

using namespace slick_socket;

class TCPServer : public TCPServerBase, public ITCPServerCallback, public ILogger
{
public:
    using TCPServerBase::TCPServerBase;

    TCPServer(const Config& config)
        : TCPServerBase(this, config, this)
    {
    }

    // Override event handlers to make them public for testing
    void onClientConnected(int client_id, const std::string& client_address) override
    {
        std::cout << "Client connected: ID=" << client_id << ", Address=" << client_address << std::endl;
    }

    void onClientDisconnected(int client_id) override
    {
        std::cout << "Client disconnected: ID=" << client_id << std::endl;
    }

    void onClientData(int client_id, const std::vector<uint8_t>& data) override
    {
        std::cout << "Data received from client ID=" << client_id << ", Size=" << data.size() << std::endl;
    }

    void logTrace(const std::string& format, std::format_args args) override
    {
        std::cout << "[TRACE] " << std::vformat(format, args) << std::endl;
    }

    void logDebug(const std::string& format, std::format_args args) override
    {
        std::cout << "[DEBUG] " << std::vformat(format, args) << std::endl;
    }

    void logInfo(const std::string& format, std::format_args args) override
    {
        std::cout << "[INFO] " << std::vformat(format, args) << std::endl;
    }

    void logWarning(const std::string& format, std::format_args args) override
    {
        std::cout << "[WARN] " << std::vformat(format, args) << std::endl;
    }

    void logError(const std::string& format, std::format_args args) override
    {
        std::cout << "[ERROR] " << std::vformat(format, args) << std::endl;
    }
};

int main()
{
    TCPServer::Config config;
    config.port = 9090; // Set custom port if needed
    config.max_connections = 50; // Set max connections
    config.receive_buffer_size = 8192; // Set receive buffer size
    config.connection_timeout = std::chrono::milliseconds(60000); // Set connection timeout

    TCPServer server(config);

    if (server.start())
    {
        std::cout << "Server started successfully on port " << config.port << std::endl;
    }
    else
    {
        std::cerr << "Failed to start server." << std::endl;
        return -1;
    }

    std::cout << "Press Enter to stop the server..." << std::endl;
    std::cin.get();

    server.stop();
    std::cout << "Server stopped." << std::endl;

    return 0;
}