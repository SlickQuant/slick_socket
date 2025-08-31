#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include "tcp_server_base.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <algorithm>

namespace slick_socket
{

template<typename DrivedT, typename LoggerT>
inline TCPServerBase<DrivedT, LoggerT>::TCPServerBase(const TCPServerConfig& config, LoggerT& logger)
    : config_(config), logger_(logger)
{
    // Ignore SIGPIPE to prevent crashes when writing to closed sockets
    std::signal(SIGPIPE, SIG_IGN);
}

template<typename DrivedT, typename LoggerT>
inline TCPServerBase<DrivedT, LoggerT>::~TCPServerBase()
{
    stop();
}

template<typename DrivedT, typename LoggerT>
inline bool TCPServerBase<DrivedT, LoggerT>::start()
{
    if (running_.load(std::memory_order_relaxed))
    {
        return true;
    }

    // Create server socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0)
    {
        logger_.logError("Failed to create server socket");
        return false;
    }

    // Set socket options
    if (config_.reuse_address)
    {
        int opt = 1;
        if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            logger_.logWarning("Failed to set SO_REUSEADDR");
        }
    }

    // Make socket non-blocking for select() usage
    int flags = fcntl(server_socket_, F_GETFL, 0);
    if (flags < 0 || fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        logger_.logWarning("Failed to make server socket non-blocking");
    }

    // Bind socket
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        logger_.logError("Failed to bind socket");
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    // Listen for connections
    if (listen(server_socket_, SOMAXCONN) < 0)
    {
        logger_.logError("Failed to listen on socket");
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }

    running_.store(true, std::memory_order_release);

    // Start single-threaded server loop
    server_thread_ = std::thread(&TCPServerBase<DrivedT, LoggerT>::server_loop, this);

    logger_.logInfo("TCP server started");
    return true;
}

template<typename DrivedT, typename LoggerT>
inline void TCPServerBase<DrivedT, LoggerT>::stop()
{
    if (!running_.load(std::memory_order_relaxed))
    {
        return;
    }

    running_.store(false, std::memory_order_release);

    // Close server socket to break select()
    if (server_socket_ >= 0)
    {
        close(server_socket_);
        server_socket_ = -1;
    }

    // Close all client sockets
    for (auto& [id, client] : clients_)
    {
        close(client.socket);
    }
    clients_.clear();

    // Wait for server thread to finish
    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

    logger_.logInfo("TCP server stopped");
}

template<typename DrivedT, typename LoggerT>
inline bool TCPServerBase<DrivedT, LoggerT>::send_data(int client_id, const std::vector<uint8_t>& data)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        return false;
    }

    ssize_t sent = send(it->second.socket, data.data(), data.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(data.size());
}

template<typename DrivedT, typename LoggerT>
inline void TCPServerBase<DrivedT, LoggerT>::disconnect_client(int client_id)
{
    auto it = clients_.find(client_id);
    if (it != clients_.end())
    {
        close(it->second.socket);
        clients_.erase(it);
    }
}

template<typename DrivedT, typename LoggerT>
void TCPServerBase<DrivedT, LoggerT>::server_loop()
{
    fd_set read_fds;
    struct timeval timeout;
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (running_.load())
    {
        // Clear and setup file descriptor set
        FD_ZERO(&read_fds);
        int max_fd = 0;

        // Add server socket
        if (server_socket_ >= 0)
        {
            FD_SET(server_socket_, &read_fds);
            max_fd = server_socket_;
        }

        // Add all client sockets
        std::vector<int> active_clients;
        for (const auto& [id, client] : clients_)
        {
            if (client.socket >= 0)
            {
                FD_SET(client.socket, &read_fds);
                if (client.socket > max_fd)
                {
                    max_fd = client.socket;
                }
                active_clients.push_back(id);
            }
        }

        // Set timeout
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // Wait for activity
        int result = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result < 0)
        {
            if (errno != EINTR)
            {
                logger_.logError("Select failed");
                break;
            }
            continue;
        }

        if (result > 0)
        {
            // Check for new connections
            if (server_socket_ >= 0 && FD_ISSET(server_socket_, &read_fds))
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
    socklen_t addr_len = sizeof(client_addr);

    int client_socket = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
    if (client_socket < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            logger_.logError("Failed to accept client");
        }
        return;
    }

    // Make client socket non-blocking
    int flags = fcntl(client_socket, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);
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

    logger_.logInfo("Client connected");
}

template<typename DrivedT, typename LoggerT>
void TCPServerBase<DrivedT, LoggerT>::handle_client_data(int client_id, std::vector<uint8_t>& buffer)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        return;
    }

    int socket = it->second.socket;
    ssize_t received = recv(socket, buffer.data(), buffer.size(), 0);

    if (received > 0)
    {
        // Process received data
        std::vector<uint8_t> data(buffer.begin(), buffer.begin() + received);

        derived().onClientData(client_id, data);
    }
    else if (received == 0)
    {
        // Client disconnected
        logger_.logInfo("Client disconnected");
        close(socket);

        // Notify about client disconnection
        derived().onClientDisconnected(client_id);

        clients_.erase(it);
    }
    else
    {
        // Error
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            logger_.logError("Receive error for client");
            close(socket);

            derived().onClientDisconnected(client_id);

            clients_.erase(it);
        }
    }
}

} // namespace slick_socket

#endif // !_WIN32 && !_WIN64
