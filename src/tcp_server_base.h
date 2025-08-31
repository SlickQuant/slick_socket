#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <ilogger.h>

namespace slick_socket
{

struct ITCPServerCallback
{
    virtual ~ITCPServerCallback() = default;

    virtual void onClientConnected(int client_id, const std::string& client_address) = 0;
    virtual void onClientDisconnected(int client_id) = 0;
    virtual void onClientData(int client_id, const std::vector<uint8_t>& data) = 0;
};

class TCPServerBase
{
public:
    struct Config
    {
        uint16_t port = 5000;
        int max_connections = 100;
        bool reuse_address = true;
        int receive_buffer_size = 4096;
        std::chrono::milliseconds connection_timeout{30000};
    };

    explicit TCPServerBase(ITCPServerCallback* callback, const Config& config = Config(), ILogger* logger = &NullLogger::instance());
    virtual ~TCPServerBase();

    // Delete copy operations
    TCPServerBase(const TCPServerBase&) = delete;
    TCPServerBase& operator=(const TCPServerBase&) = delete;

    // Move operations
    TCPServerBase(TCPServerBase&& other) noexcept;
    TCPServerBase& operator=(TCPServerBase&& other) noexcept;

    // Server control
    bool start();
    void stop();
    bool is_running() const;

protected:
    // Send data to client
    bool send_data(int client_id, const std::vector<uint8_t>& data);
    bool send_data(int client_id, const std::string& data);

    // Connection management
    void disconnect_client(int client_id);
    size_t get_connected_client_count() const;

protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace exchange_simulator
