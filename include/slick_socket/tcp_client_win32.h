#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "tcp_client.h"
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")
namespace slick_socket
{

template<typename DerivedT>
inline TCPClientBase<DerivedT>::TCPClientBase(std::string name, const TCPClientConfig& config)
    : name_(std::move(name)), config_(config)
{
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
    }
}

template<typename DerivedT>
inline TCPClientBase<DerivedT>::~TCPClientBase()
{
    disconnect();
    WSACleanup();
}

template<typename DerivedT>
inline bool TCPClientBase<DerivedT>::connect()
{
    if (connected_.load(std::memory_order_relaxed))
    {
        return true;
    }

    // Create socket
    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == invalid_socket)
    {
        LOG_ERROR("Failed to create socket");
        return false;
    }

    // Make socket non-blocking
    u_long mode = 1; // non-blocking mode
    if (ioctlsocket(socket_, FIONBIO, &mode) != 0)
    {
        LOG_WARN("Failed to make socket non-blocking");
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
        LOG_ERROR("Failed to resolve server address: {}", config_.server_address);
        closesocket(socket_);
        socket_ = invalid_socket;
        return false;
    }

    LOG_INFO("{} attempting to connect to {}:{}", name_, config_.server_address, config_.server_port);

    // Attempt to connect (non-blocking)
    int result = ::connect(socket_, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
        {
            LOG_WARN("Failed to connect to server: error {}", error);
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
        LOG_WARN("Connection timeout or failed");
        closesocket(socket_);
        socket_ = invalid_socket;
        return false;
    }
    
    connected_.store(true, std::memory_order_release);
    client_thread_ = std::thread(&TCPClientBase::client_loop, this);

    derived().onConnected();
    return true;
}

template<typename DerivedT>
inline void TCPClientBase<DerivedT>::disconnect()
{
    if (!connected_.load(std::memory_order_relaxed))
    {
        return;
    }

    LOG_INFO("Disconnecting from {}:{}...", config_.server_address, config_.server_port);
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

    LOG_INFO("Disconnected");
}

template<typename DerivedT>
inline void TCPClientBase<DerivedT>::client_loop()
{
    LOG_DEBUG("Client loop started");

    // Set CPU affinity if specified
    if (config_.cpu_affinity >= 0)
    {
        DWORD_PTR mask = 1ULL << config_.cpu_affinity;
        HANDLE thread = GetCurrentThread();
        DWORD_PTR result = SetThreadAffinityMask(thread, mask);
        if (result == 0)
        {
            LOG_WARN("Failed to set CPU affinity to core {}: error {}", 
                             config_.cpu_affinity, GetLastError());
        }
        else
        {
            LOG_INFO("Client thread pinned to CPU core {}", config_.cpu_affinity);
        }
    }

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
            LOG_INFO("Server closed connection");
            connected_.store(false, std::memory_order_release);
            break;
        }
        else
        {
            // Error or would block
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS)
            {
                LOG_ERROR("Receive error: {}", error);
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
    LOG_DEBUG("Client loop ended");
}

template<typename DerivedT>
inline bool TCPClientBase<DerivedT>::send_data(const std::vector<uint8_t>& data)
{
    if (!connected_.load(std::memory_order_relaxed) || socket_ == invalid_socket)
    {
        LOG_WARN("Cannot send data: client not connected");
        return false;
    }

    if (data.empty())
    {
        LOG_WARN("Cannot send empty data");
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

            LOG_ERROR("Failed to send data: error {}", error);

            // Check if connection is broken
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN)
            {
                LOG_INFO("Connection lost during send, disconnecting");
                disconnect();
            }
            return false;
        }

        total_sent += sent;
        
        if (sent > 0 && total_sent < data_size)
        {
            LOG_TRACE("Partial send: sent {} bytes, {} remaining", sent, data_size - total_sent);
        }
    }

    LOG_TRACE("Successfully sent {} bytes to server", total_sent);
    return true;
}

} // namespace slick_socket

#endif // defined(_WIN32) || defined(_WIN64)
