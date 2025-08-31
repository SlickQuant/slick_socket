#include <tcp_server_base.h>
#include <iostream>
#include <string>

class TCPServer : public slick_socket::TCPServerBase<TCPServer>
{
public:
    TCPServer(const slick_socket::TCPServerConfig& config)
        : slick_socket::TCPServerBase<TCPServer>(config)
    {
    }

    void onClientConnected(int client_id, const std::string& client_address)
    {
        logger_.logInfo("Client connected: ID={}, Address={}", client_id, client_address);
    }

    void onClientDisconnected(int client_id)
    {
        std::cout << "Client disconnected: ID=" << client_id << std::endl;
    }

    void onClientData(int client_id, const std::vector<uint8_t>& data)
    {
        std::cout << "Data received from client ID=" << client_id << ", Size=" << data.size() << std::endl;
    }
};

int main()
{
    slick_socket::TCPServerConfig config;
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