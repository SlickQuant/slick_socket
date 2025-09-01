#pragma once

#include "multicast_receiver.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace slick_socket
{

template<typename DerivedT, typename LoggerT>
MulticastReceiverBase<DerivedT, LoggerT>::MulticastReceiverBase(std::string name, const MulticastReceiverConfig& config, LoggerT& logger)
    : name_(std::move(name)), config_(config), logger_(logger)
{
    logger_.logDebug("MulticastReceiver {} created for group {}:{}", name_, config_.multicast_address, config_.port);
}

template<typename DerivedT, typename LoggerT>
MulticastReceiverBase<DerivedT, LoggerT>::~MulticastReceiverBase()
{
    if (running_.load(std::memory_order_relaxed))
    {
        stop();
    }
}

template<typename DerivedT, typename LoggerT>
bool MulticastReceiverBase<DerivedT, LoggerT>::start()
{
    if (running_.load(std::memory_order_relaxed))
    {
        logger_.logWarning("{} is already running", name_);
        return true;
    }

    logger_.logInfo("Starting {} for group {}:{}...", name_, config_.multicast_address, config_.port);

    // Initialize Winsock if not already done
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        logger_.logError("WSAStartup failed: {}", result);
        return false;
    }

    if (!initialize_socket())
    {
        WSACleanup();
        return false;
    }

    if (!setup_multicast_options())
    {
        cleanup_socket();
        WSACleanup();
        return false;
    }

    if (!join_multicast_group())
    {
        cleanup_socket();
        WSACleanup();
        return false;
    }

    running_.store(true, std::memory_order_relaxed);

    // Start receiver thread
    receiver_thread_ = std::thread(&MulticastReceiverBase::receiver_loop, this);

    logger_.logInfo("{} started successfully", name_);
    return true;
}

template<typename DerivedT, typename LoggerT>
void MulticastReceiverBase<DerivedT, LoggerT>::stop()
{
    if (!running_.load(std::memory_order_relaxed))
    {
        return;
    }

    logger_.logInfo("Stopping {}...", name_);
    running_.store(false, std::memory_order_relaxed);

    // Wait for receiver thread to finish
    if (receiver_thread_.joinable())
    {
        receiver_thread_.join();
    }

    leave_multicast_group();
    cleanup_socket();
    WSACleanup();

    logger_.logInfo("{} stopped", name_);
}

template<typename DerivedT, typename LoggerT>
void MulticastReceiverBase<DerivedT, LoggerT>::receiver_loop()
{
    std::vector<uint8_t> buffer(config_.receive_buffer_size);
    sockaddr_in sender_addr{};
    int sender_addr_len = sizeof(sender_addr);

    logger_.logDebug("Receiver loop started for {}", name_);

    while (running_.load(std::memory_order_relaxed))
    {
        // Set socket timeout
        DWORD timeout = static_cast<DWORD>(config_.receive_timeout.count());
        if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, 
                       reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == SOCKET_ERROR)
        {
            logger_.logWarning("Failed to set receive timeout");
        }

        int bytes_received = recvfrom(socket_, 
                                     reinterpret_cast<char*>(buffer.data()), 
                                     static_cast<int>(buffer.size()),
                                     0,
                                     reinterpret_cast<sockaddr*>(&sender_addr),
                                     &sender_addr_len);

        if (bytes_received == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT)
            {
                // Timeout is normal, continue loop
                continue;
            }
            else if (running_.load(std::memory_order_relaxed))
            {
                logger_.logError("Failed to receive multicast data. error={}", error);
                receive_errors_.fetch_add(1, std::memory_order_relaxed);
            }
            continue;
        }

        if (bytes_received > 0)
        {
            packets_received_.fetch_add(1, std::memory_order_relaxed);
            bytes_received_.fetch_add(static_cast<uint64_t>(bytes_received), std::memory_order_relaxed);

            // Get sender address as string
            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
            std::string sender_address = sender_ip;

            logger_.logTrace("Received {} bytes from {}", bytes_received, sender_address);

            // Resize buffer to actual data size and call handler
            buffer.resize(static_cast<size_t>(bytes_received));
            derived().handle_multicast_data(buffer, sender_address);
            buffer.resize(config_.receive_buffer_size); // Reset buffer size
        }
    }

    logger_.logDebug("Receiver loop ended for {}", name_);
}

template<typename DerivedT, typename LoggerT>
void MulticastReceiverBase<DerivedT, LoggerT>::handle_multicast_data(const std::vector<uint8_t>& data, const std::string& sender_address)
{
    // Default implementation - derived classes should override this
    logger_.logTrace("Received {} bytes from {} (no handler implemented)", data.size(), sender_address);
}

template<typename DerivedT, typename LoggerT>
bool MulticastReceiverBase<DerivedT, LoggerT>::initialize_socket()
{
    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == invalid_socket)
    {
        int error = WSAGetLastError();
        logger_.logError("Failed to create socket. error={}", error);
        return false;
    }

    // Set socket buffer size
    int buffer_size = config_.receive_buffer_size;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, 
                   reinterpret_cast<const char*>(&buffer_size), sizeof(buffer_size)) == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        logger_.logWarning("Failed to set receive buffer size. error={}", error);
    }

    return true;
}

template<typename DerivedT, typename LoggerT>
void MulticastReceiverBase<DerivedT, LoggerT>::cleanup_socket()
{
    if (socket_ != invalid_socket)
    {
        closesocket(socket_);
        socket_ = invalid_socket;
    }
}

template<typename DerivedT, typename LoggerT>
bool MulticastReceiverBase<DerivedT, LoggerT>::setup_multicast_options()
{
    // Enable address reuse
    if (config_.reuse_address)
    {
        DWORD reuse = 1;
        if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            logger_.logWarning("Failed to set address reuse. error={}", error);
        }
    }

    // Bind to the multicast port
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(config_.port);
    bind_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any interface

    if (bind(socket_, reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        logger_.logError("Failed to bind socket to port {}. error={}", config_.port, error);
        return false;
    }

    return true;
}

template<typename DerivedT, typename LoggerT>
bool MulticastReceiverBase<DerivedT, LoggerT>::join_multicast_group()
{
    ip_mreq mreq{};
    
    // Set multicast group address
    int result = inet_pton(AF_INET, config_.multicast_address.c_str(), &mreq.imr_multiaddr);
    if (result != 1)
    {
        logger_.logError("Invalid multicast address: {}", config_.multicast_address);
        return false;
    }

    // Set interface address
    if (config_.interface_address == "0.0.0.0")
    {
        mreq.imr_interface.s_addr = INADDR_ANY;
    }
    else
    {
        result = inet_pton(AF_INET, config_.interface_address.c_str(), &mreq.imr_interface);
        if (result != 1)
        {
            logger_.logWarning("Invalid interface address: {}, using any interface", config_.interface_address);
            mreq.imr_interface.s_addr = INADDR_ANY;
        }
    }

    // Join multicast group
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        logger_.logError("Failed to join multicast group {}. error={}", config_.multicast_address, error);
        return false;
    }

    logger_.logDebug("Joined multicast group {}", config_.multicast_address);
    return true;
}

template<typename DerivedT, typename LoggerT>
void MulticastReceiverBase<DerivedT, LoggerT>::leave_multicast_group()
{
    if (socket_ == invalid_socket)
        return;

    ip_mreq mreq{};
    
    // Set multicast group address
    int result = inet_pton(AF_INET, config_.multicast_address.c_str(), &mreq.imr_multiaddr);
    if (result == 1)
    {
        // Set interface address
        if (config_.interface_address == "0.0.0.0")
        {
            mreq.imr_interface.s_addr = INADDR_ANY;
        }
        else
        {
            inet_pton(AF_INET, config_.interface_address.c_str(), &mreq.imr_interface);
        }

        // Leave multicast group
        if (setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                       reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            logger_.logWarning("Failed to leave multicast group {}. error={}", config_.multicast_address, error);
        }
        else
        {
            logger_.logDebug("Left multicast group {}", config_.multicast_address);
        }
    }
}

} // namespace slick_socket