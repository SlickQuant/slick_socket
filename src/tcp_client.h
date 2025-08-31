#pragma once

#include <ilogger.h>
#include <vector>
#include <chrono>

namespace slick_socket
{

struct ITCPClientCallback
{
    virtual ~ITCPClientCallback() = default;

    virtual void onConnected(int client_id, const std::string& client_address) = 0;
    virtual void onDisconnected(int client_id) = 0;
    virtual void onData(int client_id, const std::vector<uint8_t>& data) = 0;
};

class TCPClient
{
public:
    struct Config
    {
        std::string server_address = "localhost";
        uint16_t server_port = 5000;
        int receive_buffer_size = 4096;
        std::chrono::milliseconds connection_timeout{30000};
        std::chrono::milliseconds reconnect_interval{5000};
    };

    explicit TCPClient(ITCPClientCallback* callback, const Config& config = Config(), ILogger* logger = &NullLogger::instance());
    virtual ~TCPClient();
};

} // namespace slick_socket

