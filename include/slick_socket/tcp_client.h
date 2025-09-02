#pragma once

#include "logger.h"
#include <vector>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif

namespace slick_socket
{

struct TCPClientConfig
{
    std::string server_address = "localhost";
    uint16_t server_port = 5000;
    int receive_buffer_size = 4096;
    std::chrono::milliseconds connection_timeout{30000};
    int cpu_affinity = -1;  // -1 means no affinity, otherwise specify CPU core index
};

template<typename DerivedT, typename LoggerT = ConsoleLogger>
class TCPClientBase
{
public:
    explicit TCPClientBase(std::string name, const TCPClientConfig& config = TCPClientConfig(), LoggerT& logger = ConsoleLogger::instance());
    virtual ~TCPClientBase();

    // Delete copy operations
    TCPClientBase(const TCPClientBase&) = delete;
    TCPClientBase& operator=(const TCPClientBase&) = delete;

    // Move operations
    TCPClientBase(TCPClientBase&& other) noexcept = default;
    TCPClientBase& operator=(TCPClientBase&& other) noexcept = default;

    bool connect();
    void disconnect();
    
    bool is_connected() const noexcept
    {
        return connected_.load(std::memory_order_relaxed);
    }

    bool send_data(const std::vector<uint8_t>& data);
    bool send_data(const std::string& data)
    {
        std::vector<uint8_t> buffer(data.begin(), data.end());
        return send_data(buffer);
    }

protected:
#if defined(_WIN32) || defined(_WIN64)
    using SocketT = SOCKET;
    static constexpr SocketT invalid_socket = INVALID_SOCKET;
#else
    using SocketT = int;
    static constexpr SocketT invalid_socket = -1;
#endif

    DerivedT& derived() { return static_cast<DerivedT&>(*this); }
    const DerivedT& derived() const { return static_cast<const DerivedT&>(*this); }

    void client_loop();
    void handle_server_data(std::vector<uint8_t>& buffer);

    std::string name_;
    TCPClientConfig config_;
    LoggerT& logger_;
    std::atomic_bool connected_{false};
    std::thread client_thread_;
    SocketT socket_ = invalid_socket;
};

} // namespace slick_socket

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_client_win32.h"
#else
#include "tcp_client_unix.h"
#endif
