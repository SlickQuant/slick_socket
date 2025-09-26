// The MIT License (MIT)
// Copyright (c) 2025 SlickQuant
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <slick_socket/logger.h>
#include <vector>
#include <thread>
#include <string>

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

template<typename DerivedT>
class TCPClientBase
{
public:
    explicit TCPClientBase(std::string name, const TCPClientConfig& config = TCPClientConfig());
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
