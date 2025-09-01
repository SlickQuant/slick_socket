#pragma once

#include "multicast_sender.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>

namespace slick_socket
{

template<typename DerivedT, typename LoggerT>
MulticastSenderBase<DerivedT, LoggerT>::MulticastSenderBase(std::string name, const MulticastSenderConfig& config, LoggerT& logger)
    : name_(std::move(name)), config_(config), logger_(logger)
{
    logger_.logDebug("MulticastSender {} created with address {}:{}", name_, config_.multicast_address, config_.port);
}

template<typename DerivedT, typename LoggerT>
MulticastSenderBase<DerivedT, LoggerT>::~MulticastSenderBase()
{
    if (running_.load(std::memory_order_relaxed))
    {
        stop();
    }
}

template<typename DerivedT, typename LoggerT>
bool MulticastSenderBase<DerivedT, LoggerT>::start()
{
    if (running_.load(std::memory_order_relaxed))
    {
        logger_.logWarning("{} is already running", name_);
        return true;
    }

    logger_.logInfo("Starting {} on {}:{}...", name_, config_.multicast_address, config_.port);

    if (!initialize_socket())
    {
        return false;
    }

    if (!setup_multicast_options())
    {
        cleanup_socket();
        return false;
    }

    running_.store(true, std::memory_order_relaxed);
    logger_.logInfo("{} started successfully", name_);
    return true;
}

template<typename DerivedT, typename LoggerT>
void MulticastSenderBase<DerivedT, LoggerT>::stop()
{
    if (!running_.load(std::memory_order_relaxed))
    {
        return;
    }

    logger_.logInfo("Stopping {}...", name_);
    running_.store(false, std::memory_order_relaxed);

    cleanup_socket();

    logger_.logInfo("{} stopped", name_);
}

template<typename DerivedT, typename LoggerT>
bool MulticastSenderBase<DerivedT, LoggerT>::send_data(const std::vector<uint8_t>& data)
{
    if (!running_.load(std::memory_order_relaxed))
    {
        logger_.logWarning("Cannot send data: {} is not running", name_);
        return false;
    }

    if (data.empty())
    {
        logger_.logWarning("Cannot send empty data");
        return false;
    }

    // Create destination address
    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config_.port);
    
    int result = inet_pton(AF_INET, config_.multicast_address.c_str(), &dest_addr.sin_addr);
    if (result != 1)
    {
        logger_.logError("Invalid multicast address: {}", config_.multicast_address);
        send_errors_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Send the data
    ssize_t bytes_sent = sendto(socket_, 
                               data.data(), 
                               data.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&dest_addr),
                               sizeof(dest_addr));

    if (bytes_sent < 0)
    {
        int error = errno;
        logger_.logError("Failed to send multicast data. error={} ({})", error, strerror(error));
        send_errors_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    if (static_cast<size_t>(bytes_sent) != data.size())
    {
        logger_.logWarning("Partial send: {} bytes sent out of {}", bytes_sent, data.size());
    }

    packets_sent_.fetch_add(1, std::memory_order_relaxed);
    bytes_sent_.fetch_add(static_cast<uint64_t>(bytes_sent), std::memory_order_relaxed);

    logger_.logTrace("Sent {} bytes to multicast group {}:{}", bytes_sent, config_.multicast_address, config_.port);
    return true;
}

template<typename DerivedT, typename LoggerT>
bool MulticastSenderBase<DerivedT, LoggerT>::initialize_socket()
{
    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == invalid_socket)
    {
        int error = errno;
        logger_.logError("Failed to create socket. error={} ({})", error, strerror(error));
        return false;
    }

    // Set socket buffer size
    int buffer_size = config_.send_buffer_size;
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
        int error = errno;
        logger_.logWarning("Failed to set send buffer size. error={} ({})", error, strerror(error));
    }

    return true;
}

template<typename DerivedT, typename LoggerT>
void MulticastSenderBase<DerivedT, LoggerT>::cleanup_socket()
{
    if (socket_ != invalid_socket)
    {
        close(socket_);
        socket_ = invalid_socket;
    }
}

template<typename DerivedT, typename LoggerT>
bool MulticastSenderBase<DerivedT, LoggerT>::setup_multicast_options()
{
    // Set TTL for multicast packets
    int ttl = config_.ttl;
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
    {
        int error = errno;
        logger_.logError("Failed to set multicast TTL. error={} ({})", error, strerror(error));
        return false;
    }

    // Set multicast loopback
    int loopback = config_.enable_loopback ? 1 : 0;
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0)
    {
        int error = errno;
        logger_.logError("Failed to set multicast loopback. error={} ({})", error, strerror(error));
        return false;
    }

    // Set multicast interface if specified
    if (config_.interface_address != "0.0.0.0")
    {
        in_addr interface_addr{};
        int result = inet_pton(AF_INET, config_.interface_address.c_str(), &interface_addr);
        if (result == 1)
        {
            if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &interface_addr, sizeof(interface_addr)) < 0)
            {
                int error = errno;
                logger_.logError("Failed to set multicast interface. error={} ({})", error, strerror(error));
                return false;
            }
        }
        else
        {
            logger_.logWarning("Invalid interface address: {}, using default", config_.interface_address);
        }
    }

    return true;
}

} // namespace slick_socket