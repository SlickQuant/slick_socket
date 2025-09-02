#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "tcp_client.h"
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
namespace slick_socket
{

template<typename DerivedT, typename LoggerT>
inline TCPClientBase<DerivedT, LoggerT>::TCPClientBase(std::string name, const TCPClientConfig& config, LoggerT& logger)
    : name_(std::move(name)), config_(config), logger_(logger)
{
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
    }
}

template<typename DerivedT, typename LoggerT>
inline TCPClientBase<DerivedT, LoggerT>::~TCPClientBase()
{
    disconnect();
    WSACleanup();
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
        logger_.logError("Failed to create socket");
        return false;
    }

    // Make socket non-blocking
    u_long mode = 1; // non-blocking mode
    if (ioctlsocket(socket_, FIONBIO, &mode) != 0)
    {
        logger_.logWarning("Failed to make socket non-blocking");
        closesocket(socket_);
        socket_ = invalid_socket;
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
        closesocket(socket_);
        socket_ = invalid_socket;
        return false;
    }

    logger_.logInfo("{} attempting to connect to {}:{}", name_, config_.server_address, config_.server_port);

    // Attempt to connect (non-blocking)
    int result = ::connect(socket_, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
        {
            logger_.logWarning("Failed to connect to server: error {}", error);
            closesocket(socket_);
            socket_ = invalid_socket;
            return false;
        }
    }

    // Wait for connection to complete
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(socket_, &write_fds);

    struct timeval timeout;
    timeout.tv_sec = static_cast<long>(config_.connection_timeout.count() / 1000);
    timeout.tv_usec = static_cast<long>((config_.connection_timeout.count() % 1000) * 1000);

    result = select(static_cast<int>(socket_) + 1, nullptr, &write_fds, nullptr, &timeout);
    if (result <= 0)
    {
        logger_.logWarning("Connection timeout or failed");
        closesocket(socket_);
        socket_ = invalid_socket;
        return false;
    }
    
    connected_.store(true, std::memory_order_release);
    client_thread_ = std::thread(&TCPClientBase::client_loop, this);

    derived().onConnected();
    logger_.logInfo("Successfully connected to server");
    return true;
}

template<typename DerivedT, typename LoggerT>
inline void TCPClientBase<DerivedT, LoggerT>::disconnect()
{
    if (!connected_.load(std::memory_order_relaxed))
    {
        return;
    }

    logger_.logInfo("Disconnecting from {}:{}...", config_.server_address, config_.server_port);
    connected_.store(false, std::memory_order_release);

    // Close socket if open
    if (socket_ != invalid_socket)
    {
        closesocket(socket_);
        socket_ = invalid_socket;
    }

    // Wait for client thread to finish
    if (client_thread_.joinable())
    {
        client_thread_.join();
    }

    logger_.logInfo("Disconnected");
}

template<typename DerivedT, typename LoggerT>
inline void TCPClientBase<DerivedT, LoggerT>::client_loop()
{
    logger_.logDebug("Client loop started");

    // Connection established - handle server communication
    std::vector<uint8_t> buffer(config_.receive_buffer_size);

    while (connected_.load(std::memory_order_relaxed))
    {
        // Check for incoming data (non-blocking)
        int received = recv(socket_, (char*)buffer.data(), (int)buffer.size(), 0);
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
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
            {
                logger_.logError("Receive error: {}", error);
                connected_.store(false, std::memory_order_release);
                break;
            }
        }
        std::this_thread::yield();
    }

    derived().onDisconnected();

    // Connection lost - clean up
    closesocket(socket_);
    socket_ = invalid_socket;
    logger_.logDebug("Client loop ended");
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
    const char* buffer = reinterpret_cast<const char*>(data.data());

    // Keep sending until all data is sent
    while (total_sent < data_size)
    {
        int sent = send(socket_, buffer + total_sent, static_cast<int>(data_size - total_sent), 0);
        if (sent == SOCKET_ERROR)
        {
            int error = WSAGetLastError();

            // Check for non-blocking specific errors
            if (error == WSAEWOULDBLOCK)
            {
                // Socket buffer is full, retry immediately
                continue;
            }

            logger_.logError("Failed to send data: error {}", error);

            // Check if connection is broken
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN)
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

#endif // defined(_WIN32) || defined(_WIN64)
