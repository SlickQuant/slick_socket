#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "tcp_server_base.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <queue>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace slick_socket
{

class TCPServerBase::Impl
{
    friend class TCPServerBase;
public:
    Impl(const Config& config, ITCPServerCallback* callback, ILogger* logger = &NullLogger::instance()) 
        : logger_(*logger), callback_(*callback), config_(config), running_(false)
    {
        initialize_winsock();
    }

    ~Impl()
    {
        stop();
        WSACleanup();
    }

    bool start()
    {
        if (running_.load(std::memory_order_relaxed))
        {
            return true;
        }

        // Create server socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket_ == INVALID_SOCKET)
        {
            logger_.logError("Failed to create server socket");
            return false;
        }

        // Set socket options
        if (config_.reuse_address)
        {
            int opt = 1;
            setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        }

        // Bind socket
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config_.port);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
        {
            logger_.logError("Failed to bind socket");
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        // Listen for connections
        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR)
        {
            logger_.logError("Failed to listen on socket");
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        running_.store(true, std::memory_order_release);

        // Start single-threaded server loop
        server_thread_ = std::thread(&Impl::server_loop, this);

        logger_.logInfo("TCP server started");
        return true;
    }

    void stop()
    {
        if (!running_.load(std::memory_order_relaxed))
        {
            return;
        }

        running_.store(false, std::memory_order_release);

        // Close server socket to break select()
        if (server_socket_ != INVALID_SOCKET)
        {
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }

        // Close all client sockets
        for (auto& [id, client] : clients_)
        {
            closesocket(client.socket);
        }
        clients_.clear();

        // Wait for server thread to finish
        if (server_thread_.joinable())
        {
            server_thread_.join();
        }

        logger_.logInfo("TCP server stopped");
    }

    bool is_running() const
    {
        return running_.load();
    }

    bool send_data(int client_id, const std::vector<uint8_t>& data)
    {
        auto it = clients_.find(client_id);
        if (it == clients_.end())
        {
            return false;
        }

        int sent = send(it->second.socket, (char*)data.data(), (int)data.size(), 0);
        return sent == (int)data.size();
    }

    bool send_data(int client_id, const std::string& data)
    {
        std::vector<uint8_t> buffer(data.begin(), data.end());
        return send_data(client_id, buffer);
    }

    void disconnect_client(int client_id)
    {
        auto it = clients_.find(client_id);
        if (it != clients_.end())
        {
            closesocket(it->second.socket);
            clients_.erase(it);
        }
    }

    size_t get_connected_client_count() const
    {
        return clients_.size();
    }

private:
    struct ClientInfo
    {
        SOCKET socket;
        std::string address;
    };

    ILogger& logger_;
    ITCPServerCallback& callback_;
    Config config_;
    std::atomic_bool running_;
    std::thread server_thread_;
    SOCKET server_socket_ = INVALID_SOCKET;

    std::unordered_map<int, ClientInfo> clients_;
    std::atomic<int> next_client_id_{1};

    void initialize_winsock()
    {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0)
        {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
    }

    void server_loop()
    {
        fd_set read_fds;
        struct timeval timeout;
        std::vector<uint8_t> buffer(config_.receive_buffer_size);

        while (running_.load())
        {
            // Clear and setup file descriptor set
            FD_ZERO(&read_fds);
            int max_fd = 0;

            // Add server socket
            if (server_socket_ != INVALID_SOCKET)
            {
                FD_SET(server_socket_, &read_fds);
                max_fd = server_socket_;
            }

            // Add all client sockets
            std::vector<int> active_clients;
            for (const auto& [client_id, client_info] : clients_)
            {
                if (client_info.socket != INVALID_SOCKET)
                {
                    FD_SET(client_info.socket, &read_fds);
                    if (client_info.socket > max_fd)
                    {
                        max_fd = client_info.socket;
                    }
                    active_clients.push_back(client_id);
                }
            }

            // Set timeout
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            // Wait for activity
            int result = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
            if (result == SOCKET_ERROR)
            {
                logger_.logError("Select failed");
                break;
            }

            if (result > 0)
            {
                // Check for new connections
                if (FD_ISSET(server_socket_, &read_fds))
                {
                    accept_new_client();
                }

                // Check for client data
                for (int client_id : active_clients)
                {
                    auto it = clients_.find(client_id);
                    if (it != clients_.end() && FD_ISSET(it->second.socket, &read_fds))
                    {
                        handle_client_data(client_id, buffer);
                    }
                }
            }
        }
    }

    void accept_new_client()
    {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET)
        {
            logger_.logError("Failed to accept client");
            return;
        }

        // Get client address
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, INET_ADDRSTRLEN);

        int client_id = next_client_id_.fetch_add(1);
        std::string client_address = addr_str;

        // Add client to map
        clients_[client_id] = {client_socket, client_address};

        // Notify about new client
        callback_.onClientConnected(client_id, client_address);

        logger_.logInfo("Client connected");
    }

    void handle_client_data(int client_id, std::vector<uint8_t>& buffer)
    {
        auto it = clients_.find(client_id);
        if (it == clients_.end())
        {
            return;
        }

        SOCKET socket = it->second.socket;
        int received = recv(socket, (char*)buffer.data(), (int)buffer.size(), 0);

        if (received > 0)
        {
            // Process received data
            std::vector<uint8_t> data(buffer.begin(), buffer.begin() + received);

            callback_.onClientData(client_id, data);
        }
        else if (received == 0)
        {
            // Client disconnected
            logger_.logInfo("Client disconnected");
            closesocket(socket);

            // Notify about client disconnection
            callback_.onClientDisconnected(client_id);

            clients_.erase(it);
        }
        else
        {
            // Error
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                logger_.logError("Receive error for client");
                closesocket(socket);

                callback_.onClientDisconnected(client_id);

                clients_.erase(it);
            }
        }
    }
};

} // namespace slick_socket


// TCPServer implementation
#include "tcp_server_base_imp.h"

#endif // _WIN32
