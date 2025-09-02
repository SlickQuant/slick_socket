#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include "tcp_client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <pthread.h>

namespace slick_socket
{

template<typename DerivedT, typename LoggerT>
inline TCPClientBase<DerivedT, LoggerT>::TCPClientBase(std::string name, const TCPClientConfig& config, LoggerT& logger)
    : name_(std::move(name)),config_(config), logger_(logger)
{
    // Ignore SIGPIPE to prevent crashes when writing to closed sockets
    std::signal(SIGPIPE, SIG_IGN);
}

template<typename DerivedT, typename LoggerT>
inline TCPClientBase<DerivedT, LoggerT>::~TCPClientBase()
{
    disconnect();
}

template<typename DerivedT, typename LoggerT>
inline bool TCPClientBase<DerivedT, LoggerT>::connect()
{
    if (connected_.load(std::memory_order_relaxed))
    {
        return true;
    }

    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == invalid_socket)
    {
        logger_.logError("Failed to create socket: {}", std::strerror(errno));
        return false;
    }

    // Make socket non-blocking
    int flags = fcntl(socket_, F_GETFL, 0);
    if (flags < 0 || fcntl(socket_, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        logger_.logWarning("Failed to make socket non-blocking: {}", std::strerror(errno));
        return false;
    }

    // Set up server address
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config_.server_port);

    // Resolve server address
    if (inet_pton(AF_INET, config_.server_address.c_str(), &server_addr.sin_addr) != 1)
    {
        logger_.logError("Failed to resolve server address: {}", config_.server_address);
        close(socket_);
        socket_ = invalid_socket;
        return false;
    }

    logger_.logInfo("Attempting to connect to {}:{}", config_.server_address, config_.server_port);

    // Attempt to connect (non-blocking)
    int result = ::connect(socket_, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result < 0 && errno != EINPROGRESS)
    {
        logger_.logWarning("Failed to connect to server: {}", std::strerror(errno));
        close(socket_);
        socket_ = invalid_socket;
        return false;
    }

    // Wait for connection to complete
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(socket_, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = config_.connection_timeout.count() / 1000;
    timeout.tv_usec = (config_.connection_timeout.count() % 1000) * 1000;

    result = select(socket_ + 1, nullptr, &write_fds, nullptr, &timeout);
    if (result <= 0)
    {
        logger_.logWarning("Connection timeout or failed");
        close(socket_);
        socket_ = invalid_socket;
        return false;
    }

    // Check if connection was successful
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
    {
        logger_.logWarning("Connection failed: {}", std::strerror(error));
        close(socket_);
        socket_ = invalid_socket;
        return false;
    }

    connected_.store(true, std::memory_order_release);
    client_thread_ = std::thread(&TCPClientBase::client_loop, this);
    derived().onConnected();
    return true;
}

template<typename DerivedT, typename LoggerT>
inline void TCPClientBase<DerivedT, LoggerT>::disconnect()
{
    if (!connected_.load(std::memory_order_relaxed))
    {
        return;
    }

    connected_.store(false, std::memory_order_release);

    // Close socket if open
    if (socket_ != invalid_socket)
    {
        close(socket_);
        socket_ = invalid_socket;
    }

    // Wait for client thread to finish
    if (client_thread_.joinable())
    {
        client_thread_.join();
    }

    logger_.logInfo("TCP client disconnected");
}

template<typename DerivedT, typename LoggerT>
inline void TCPClientBase<DerivedT, LoggerT>::client_loop()
{
    logger_.logInfo("Client loop started");

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
            logger_.logInfo("Client thread pinned to CPU core {}", config_.cpu_affinity);
        }
    }

    // Connection established - handle server communication
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (connected_.load(std::memory_order_relaxed))
    {
        // Check for incoming data (non-blocking)
        ssize_t received = recv(socket_, (char*)buffer.data(), buffer.size(), 0);

        if (received > 0)
        {
            // Process received data
            derived().onData(buffer.data(), received);
            continue;
        }
        else if (received == 0)
        {
            // Server closed connection
            logger_.logInfo("Server closed connection");
            connected_.store(false, std::memory_order_release);
            break;
        }
        else
        {
            // Error or would block
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                logger_.logError("Receive error: {}", std::strerror(errno));
                connected_.store(false, std::memory_order_release);
                break;
            }
        }
    }

    derived().onDisconnected();

    // Connection lost - clean up
    close(socket_);
    socket_ = invalid_socket;
    logger_.logInfo("Client loop ended");
}

template<typename DerivedT, typename LoggerT>
inline void TCPClientBase<DerivedT, LoggerT>::handle_server_data(std::vector<uint8_t>& buffer)
{
    // Default implementation - derived classes should override this
    logger_.logDebug("Received {} bytes from server", buffer.size());
}

template<typename DerivedT, typename LoggerT>
inline bool TCPClientBase<DerivedT, LoggerT>::send_data(const std::vector<uint8_t>& data)
{
    if (!connected_.load(std::memory_order_relaxed) || socket_ == invalid_socket)
    {
        logger_.logWarning("Cannot send data: client not connected");
        return false;
    }

    if (data.empty())
    {
        logger_.logWarning("Cannot send empty data");
        return false;
    }

    size_t total_sent = 0;
    size_t data_size = data.size();
    const uint8_t* buffer = data.data();

    // Keep sending until all data is sent
    while (total_sent < data_size)
    {
        ssize_t sent = send(socket_, buffer + total_sent, data_size - total_sent, MSG_NOSIGNAL);
        if (sent < 0)
        {
            // Check for non-blocking specific errors
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Socket buffer is full, retry immediately
                continue;
            }

            logger_.logError("Failed to send data: {}", std::strerror(errno));

            // Check if connection is broken
            if (errno == ECONNRESET || errno == EPIPE || errno == ENOTCONN)
            {
                logger_.logInfo("Connection lost during send, disconnecting");
                disconnect();
            }
            return false;
        }

        total_sent += sent;
        
        if (sent > 0 && total_sent < data_size)
        {
            logger_.logTrace("Partial send: sent {} bytes, {} remaining", sent, data_size - total_sent);
        }
    }

    logger_.logTrace("Successfully sent {} bytes to server", total_sent);
    return true;
}

} // namespace slick_socket

#endif // !defined(_WIN32) && !defined(_WIN64)
