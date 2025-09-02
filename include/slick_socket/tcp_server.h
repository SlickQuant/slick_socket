#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <string>
#include "logger.h"

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#endif

namespace slick_socket
{

struct TCPServerConfig
{
    uint16_t port = 5000;
    int max_connections = 100;
    bool reuse_address = true;
    int receive_buffer_size = 4096;
    std::chrono::milliseconds connection_timeout{30000};
    int cpu_affinity = -1;  // -1 means no affinity, otherwise specify CPU core index
};

template<typename DerivedT, typename LoggerT = ConsoleLogger>
class TCPServerBase
{
public:
    explicit TCPServerBase(std::string name, const TCPServerConfig& config = TCPServerConfig(), LoggerT& logger = ConsoleLogger::instance());
    virtual ~TCPServerBase();

    // Delete copy operations
    TCPServerBase(const TCPServerBase&) = delete;
    TCPServerBase& operator=(const TCPServerBase&) = delete;

    // Move operations
    TCPServerBase(TCPServerBase&& other) noexcept = default;
    TCPServerBase& operator=(TCPServerBase&& other) noexcept = default;

    // Server control
    bool start();
    void stop();

    bool is_running() const noexcept
    {
        return running_.load(std::memory_order_relaxed);
    }

protected:
    DerivedT& derived() { return static_cast<DerivedT&>(*this); }
    const DerivedT& derived() const { return static_cast<const DerivedT&>(*this); }

    void server_loop();
    void accept_new_client();
    void handle_client_data(int client_id, std::vector<uint8_t>& buffer);

    // Send data to client
    bool send_data(int client_id, const std::vector<uint8_t>& data);
    bool send_data(int client_id, const std::string& data)
    {
        std::vector<uint8_t> buffer(data.begin(), data.end());
        return send_data(client_id, buffer);
    }

    // Connection management
    void disconnect_client(int client_id);
    
    size_t get_connected_client_count() const noexcept
    {   
        return clients_.size();
    }

#if defined(_WIN32) || defined(_WIN64)
    using SocketT = SOCKET;
    static constexpr SocketT invalid_socket = INVALID_SOCKET;
#else
    using SocketT = int;
    static constexpr SocketT invalid_socket = -1;
#endif

    void close_socket(SocketT socket);

protected:

    struct ClientInfo
    {
        SocketT socket;
        std::string address;
    };

    std::string name_;
    TCPServerConfig config_;
    LoggerT& logger_;
    std::atomic_bool running_{false};

    std::thread server_thread_;
    SocketT server_socket_ = invalid_socket;

#if !defined(_WIN32) && !defined(_WIN64)
    int epoll_fd_ = -1;  // epoll file descriptor for Unix/Linux
#else
    HANDLE epoll_fd_ = nullptr;  // wepoll handle for Windows (epoll-like API)
#endif

    std::unordered_map<int, ClientInfo> clients_;
    std::unordered_map<SocketT, int> socket_to_client_id_;
    std::atomic<int> next_client_id_{1};
};

} // namespace slick_socket

#if defined(_WIN32) || defined(_WIN64)
#include "tcp_server_win32.h"
#else
#include "tcp_server_unix.h"
#endif
