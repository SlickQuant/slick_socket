#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include "tcp_server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <algorithm>
#include <cstring>
#include <pthread.h>
#include <sys/epoll.h>

namespace slick_socket
{

template<typename DerivedT, typename LoggerT>
inline TCPServerBase<DerivedT, LoggerT>::TCPServerBase(std::string name, const TCPServerConfig& config, LoggerT& logger)
    : name_(std::move(name)), config_(config), logger_(logger)
{
    // Ignore SIGPIPE to prevent crashes when writing to closed sockets
    std::signal(SIGPIPE, SIG_IGN);
}

template<typename DerivedT, typename LoggerT>
inline TCPServerBase<DerivedT, LoggerT>::~TCPServerBase()
{
    stop();

    if (server_socket_ >= 0)
    {
        close(server_socket_);
        server_socket_ = -1;
    }

    for (auto& [id, client] : clients_)
    {
        close(client.socket);
    }
    clients_.clear();
    socket_to_client_id_.clear();

    if (epoll_fd_ >= 0)
    {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

template<typename DerivedT, typename LoggerT>
inline bool TCPServerBase<DerivedT, LoggerT>::start()
{
    if (running_.load(std::memory_order_relaxed))
    {
        return true;
    }

    logger_.logInfo("Starting {}, lisening on: {}...", name_, config_.port);
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
    server_thread_ = std::thread(&TCPServerBase<DerivedT, LoggerT>::server_loop, this);

    logger_.logInfo("{} started", name_);
    return true;
}

template<typename DerivedT, typename LoggerT>
inline void TCPServerBase<DerivedT, LoggerT>::stop()
{
    if (!running_.load(std::memory_order_relaxed))
    {
        return;
    }

    logger_.logInfo("Stopping {}...", name_);
    running_.store(false, std::memory_order_release);

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
    socket_to_client_id_.clear();

    // Wait for server thread to finish
    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

    logger_.logInfo("{} stopped", name_);
}

template<typename DerivedT, typename LoggerT>
inline bool TCPServerBase<DerivedT, LoggerT>::send_data(int client_id, const std::vector<uint8_t>& data)
{
    auto it = clients_.find(client_id);
    if (it == clients_.end())
    {
        return false;
    }

    size_t total_sent = 0;
    size_t data_size = data.size();
    const uint8_t* buffer = data.data();

    // Keep sending until all data is sent
    while (total_sent < data_size)
    {
        ssize_t sent = send(it->second.socket, buffer + total_sent, data_size - total_sent, MSG_NOSIGNAL);
        if (sent < 0)
        {
            // Check for non-blocking specific errors
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Socket buffer is full, retry immediately
                continue;
            }

            logger_.logError("Failed to send data to client {}: {}", client_id, std::strerror(errno));

            // Check if connection is broken
            if (errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN)
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

template<typename DerivedT, typename LoggerT>
inline void TCPServerBase<DerivedT, LoggerT>::close_socket(SocketT socket)
{
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket, nullptr);
    socket_to_client_id_.erase(socket);
    close(socket);
}

template<typename DerivedT, typename LoggerT>
inline void TCPServerBase<DerivedT, LoggerT>::disconnect_client(int client_id)
{
    auto it = clients_.find(client_id);
    if (it != clients_.end())
    {
        close_socket(it->second.socket);
        clients_.erase(it);
    }
}

template<typename DerivedT, typename LoggerT>
void TCPServerBase<DerivedT, LoggerT>::server_loop()
{
    // Set CPU affinity if specified
    if (config_.cpu_affinity >= 0)
    {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(config_.cpu_affinity, &cpuset);
        
        pthread_t thread = pthread_self();
        int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (result != 0)
        {
            logger_.logWarning("Failed to set CPU affinity to core {}: {}", 
                             config_.cpu_affinity, std::strerror(result));
        }
        else
        {
            logger_.logInfo("Server thread pinned to CPU core {}", config_.cpu_affinity);
        }
    }

    // Create epoll instance
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0)
    {
        logger_.logError("Failed to create epoll instance: {}", std::strerror(errno));
        return;
    }

    // Add server socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_socket_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_socket_, &ev) < 0)
    {
        logger_.logError("Failed to add server socket to epoll: {}", std::strerror(errno));
        close(epoll_fd_);
        epoll_fd_ = -1;
        return;
    }

    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (running_.load())
    {
        // Wait for events with 1 second timeout
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, 0);
        if (num_events < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            logger_.logError("epoll_wait failed: {}", std::strerror(errno));
            break;
        }

        for (int i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_socket_)
            {
                // New connection on server socket
                accept_new_client();
            }
            else
            {
                auto it = socket_to_client_id_.find(events[i].data.fd);
                if (it != socket_to_client_id_.end())
                {
                    handle_client_data(it->second, buffer);
                }
            }
        }
    }

    // Clean up
    if (epoll_fd_ != nullptr)
    {
        epoll_close(epoll_fd_);
        epoll_fd_ = nullptr;
    }
}

template<typename DerivedT, typename LoggerT>
void TCPServerBase<DerivedT, LoggerT>::accept_new_client()
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

    // Add client socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
    ev.data.fd = client_socket;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_socket, &ev) < 0)
    {
        logger_.logError("Failed to add client socket to epoll: {}", std::strerror(errno));
        close(client_socket);
        return;
    }

    // Get client address
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);

    uint32_t client_id = next_client_id_.fetch_add(1);
    std::string client_address = addr_str;

    // Add client to maps
    clients_[client_id] = {client_socket, client_address};
    socket_to_client_id_[client_socket] = client_id;

    // Notify about new client
    derived().onClientConnected(client_id, client_address);
}

template<typename DerivedT, typename LoggerT>
void TCPServerBase<DerivedT, LoggerT>::handle_client_data(int client_id, std::vector<uint8_t>& buffer)
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
        derived().onClientData(client_id, buffer.data(), received);
    }
    else if (received == 0)
    {
        // Client disconnected
        close_socket(socket);
        clients_.erase(it);
        // Notify about client disconnection
        derived().onClientDisconnected(client_id);
    }
    else
    {
        // Error
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            logger_.logError("Receive error for client ID={}", client_id);
            close_socket(socket);
            clients_.erase(it);
            derived().onClientDisconnected(client_id);
        }
    }
}

} // namespace slick_socket

#endif // !_WIN32 && !_WIN64
