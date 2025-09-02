#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "tcp_server.h"
#include <ws2tcpip.h>
#include <queue>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace slick_socket
{

template<typename DrivedT, typename LoggerT>
inline TCPServerBase<DrivedT, LoggerT>::TCPServerBase(std::string name, const TCPServerConfig& config, LoggerT& logger)
    : name_(std::move(name)), config_(config), logger_(logger)
{
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
    }
}

template<typename DrivedT, typename LoggerT>
inline TCPServerBase<DrivedT, LoggerT>::~TCPServerBase()
{
    stop();
    WSACleanup();
}

template<typename DrivedT, typename LoggerT>
inline bool TCPServerBase<DrivedT, LoggerT>::start()
{
    if (running_.load(std::memory_order_relaxed))
    {
        return true;
    }

    logger_.logInfo("Starting {}, lisening on: {}...", name_, config_.port);
    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET)
    {
        logger_.logError("Failed to create server socket");
        return false;
    }

    // Make server socket non-blocking
    u_long mode = 1; // non-blocking mode
    if (ioctlsocket(server_socket_, FIONBIO, &mode) != 0)
    {
        logger_.logWarning("Failed to make server socket non-blocking");
    }

    // Set socket options
    if (config_.reuse_address)
    {
        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    }

    // Bind socket
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        logger_.logError("Failed to bind socket");
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }

    // Listen for connections
    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR)
    {
        logger_.logError("Failed to listen on socket");
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
        return false;
    }

    running_.store(true, std::memory_order_release);

    // Start single-threaded server loop
    server_thread_ = std::thread(&TCPServerBase<DrivedT, LoggerT>::server_loop, this);

    logger_.logInfo("{} started", name_);
    return true;
}

template<typename DrivedT, typename LoggerT>
inline void TCPServerBase<DrivedT, LoggerT>::stop()
{
    if (!running_.load(std::memory_order_relaxed))
    {
        return;
    }

    logger_.logInfo("Stopping {}...", name_);
    running_.store(false, std::memory_order_release);

    // Close server socket to break select()
    if (server_socket_ != INVALID_SOCKET)
    {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }

    // Close all client sockets
    for (auto& [client_id, client_info] : clients_)
    {
        closesocket(client_info.socket);
    }
    clients_.clear();

    // Wait for server thread to finish
    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

    logger_.logInfo("{} stopped", name_);
}

template<typename DrivedT, typename LoggerT>
inline bool TCPServerBase<DrivedT, LoggerT>::send_data(int client_id, const std::vector<uint8_t>& data)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        return false;
    }

    size_t total_sent = 0;
    size_t data_size = data.size();
    const char* buffer = reinterpret_cast<const char*>(data.data());

    // Keep sending until all data is sent
    while (total_sent < data_size)
    {
        int sent = send(it->second.socket, buffer + total_sent, static_cast<int>(data_size - total_sent), 0);
        if (sent == SOCKET_ERROR)
        {
            int error = WSAGetLastError();

            // Check for non-blocking specific errors
            if (error == WSAEWOULDBLOCK)
            {
                // Socket buffer is full, retry immediately
                continue;
            }

            logger_.logError("Failed to send data to client {}: error {}", client_id, error);

            // Check if connection is broken
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN)
            {
                logger_.logInfo("Connection lost during send to client {}, disconnecting", client_id);
                disconnect_client(client_id);
            }
            return false;
        }

        total_sent += sent;
        
        if (sent > 0 && total_sent < data_size)
        {
            logger_.logTrace("Partial send to client {}: sent {} bytes, {} remaining", 
                           client_id, sent, data_size - total_sent);
        }
    }

    logger_.logTrace("Successfully sent {} bytes to client {}", total_sent, client_id);
    return true;
}

template<typename DrivedT, typename LoggerT>
inline void TCPServerBase<DrivedT, LoggerT>::disconnect_client(int client_id)
{
    auto it = clients_.find(client_id);
    if (it != clients_.end())
    {
        closesocket(it->second.socket);
        clients_.erase(it);
    }
}

template<typename DrivedT, typename LoggerT>
void TCPServerBase<DrivedT, LoggerT>::server_loop()
{
    fd_set read_fds;
    struct timeval timeout;
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (running_.load(std::memory_order_relaxed))
    {
        // Clear and setup file descriptor set
        FD_ZERO(&read_fds);
        int max_fd = 0;

        // Add server socket
        if (server_socket_ != INVALID_SOCKET)
        {
            FD_SET(server_socket_, &read_fds);
            max_fd = server_socket_;
        }

        // Add all client sockets
        std::vector<int> active_clients;
        for (const auto& [client_id, client_info] : clients_)
        {
            if (client_info.socket != INVALID_SOCKET)
            {
                FD_SET(client_info.socket, &read_fds);
                if (client_info.socket > max_fd)
                {
                    max_fd = client_info.socket;
                }
                active_clients.push_back(client_id);
            }
        }

        // Set timeout
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Wait for activity
        int result = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result == SOCKET_ERROR)
        {
            logger_.logError("Select failed");
            break;
        }

        if (result > 0)
        {
            // Check for new connections
            if (FD_ISSET(server_socket_, &read_fds))
            {
                accept_new_client();
            }

            // Check for client data
            for (int client_id : active_clients)
            {
                auto it = clients_.find(client_id);
                if (it != clients_.end() && FD_ISSET(it->second.socket, &read_fds))
                {
                    handle_client_data(client_id, buffer);
                }
            }
        }
    }
}

template<typename DrivedT, typename LoggerT>
void TCPServerBase<DrivedT, LoggerT>::accept_new_client()
{
    sockaddr_in client_addr{};
    int addr_len = sizeof(client_addr);

    SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
    if (client_socket == INVALID_SOCKET)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAENOTSOCK)
        {
            logger_.logError("Failed to accept client. error={}", error);
        }
        return;
    }

    // Make client socket non-blocking
    u_long mode = 1; // non-blocking mode
    if (ioctlsocket(client_socket, FIONBIO, &mode) != 0)
    {
        logger_.logWarning("Failed to make client socket non-blocking");
        closesocket(client_socket);
        return;
    }

    // Get client address
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);

    int client_id = next_client_id_.fetch_add(1);
    std::string client_address = addr_str;

    // Add client to map
    clients_[client_id] = {client_socket, client_address};

    // Notify about new client
    derived().onClientConnected(client_id, client_address);
    logger_.logInfo("{} client connected: ID={}, Address={}", name_, client_id, client_address);
}

template<typename DrivedT, typename LoggerT>
void TCPServerBase<DrivedT, LoggerT>::handle_client_data(int client_id, std::vector<uint8_t>& buffer)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        return;
    }

    SOCKET socket = it->second.socket;
    int received = recv(socket, (char*)buffer.data(), (int)buffer.size(), 0);

    if (received > 0)
    {
        derived().onClientData(client_id, buffer.data(), received);
    }
    else if (received == 0)
    {
        // Client disconnected
        closesocket(socket);
        // Notify about client disconnection
        derived().onClientDisconnected(client_id);
        logger_.logInfo("{} client disconnected: ID={}", name_, client_id);
        clients_.erase(it);
    }
    else
    {
        // Error
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK)
        {
            logger_.logError("Receive error for client");
            closesocket(socket);
            derived().onClientDisconnected(client_id);
            logger_.logInfo("{} client disconnected: ID={}", name_, client_id);
            clients_.erase(it);
        }
    }
}

} // namespace slick_socket

#endif // _WIN32
