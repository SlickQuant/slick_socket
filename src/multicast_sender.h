#pragma once

#include "logger.h"
#include <vector>
#include <string>
#include <chrono>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace slick_socket
{

struct MulticastSenderConfig
{
    std::string multicast_address = "224.0.0.1"; // Default multicast address
    uint16_t port = 5000;
    std::string interface_address = "0.0.0.0"; // Interface to send from (0.0.0.0 = any)
    int ttl = 1; // Time-to-live for multicast packets
    bool enable_loopback = false; // Enable loopback of multicast packets
    int send_buffer_size = 65536; // Socket send buffer size
};

template<typename DerivedT, typename LoggerT = ConsoleLogger>
class MulticastSenderBase
{
public:
    explicit MulticastSenderBase(std::string name, const MulticastSenderConfig& config = MulticastSenderConfig(), LoggerT& logger = ConsoleLogger::instance());
    virtual ~MulticastSenderBase();

    // Delete copy operations
    MulticastSenderBase(const MulticastSenderBase&) = delete;
    MulticastSenderBase& operator=(const MulticastSenderBase&) = delete;

    // Move operations
    MulticastSenderBase(MulticastSenderBase&& other) noexcept = default;
    MulticastSenderBase& operator=(MulticastSenderBase&& other) noexcept = default;

    // Sender control
    bool start();
    void stop();

    bool is_running() const noexcept
    {
        return running_.load(std::memory_order_relaxed);
    }

    // Send data
    bool send_data(const std::vector<uint8_t>& data);
    bool send_data(const std::string& data)
    {
        std::vector<uint8_t> buffer(data.begin(), data.end());
        return send_data(buffer);
    }

    // Statistics
    uint64_t get_packets_sent() const noexcept
    {
        return packets_sent_.load(std::memory_order_relaxed);
    }

    uint64_t get_bytes_sent() const noexcept
    {
        return bytes_sent_.load(std::memory_order_relaxed);
    }

    uint64_t get_send_errors() const noexcept
    {
        return send_errors_.load(std::memory_order_relaxed);
    }

protected:
    DerivedT& derived() { return static_cast<DerivedT&>(*this); }
    const DerivedT& derived() const { return static_cast<const DerivedT&>(*this); }

#if defined(_WIN32) || defined(_WIN64)
    using SocketT = SOCKET;
    static constexpr SocketT invalid_socket = INVALID_SOCKET;
#else
    using SocketT = int;
    static constexpr SocketT invalid_socket = -1;
#endif

    std::string name_;
    MulticastSenderConfig config_;
    LoggerT& logger_;
    std::atomic_bool running_{false};

    SocketT socket_ = invalid_socket;
    
    // Statistics
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> send_errors_{0};

private:
    bool initialize_socket();
    void cleanup_socket();
    bool setup_multicast_options();
};

} // namespace slick_socket

#if defined(_WIN32) || defined(_WIN64)
#include "multicast_sender_win32.h"
#else
#include "multicast_sender_unix.h"
#endif