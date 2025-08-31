#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <network/tcp_server.h>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

namespace slick_socket::test
{

// Base test fixture for TCPServer tests
class TCPServerTestBase : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Default test configuration
        config_.port = 0; // Use any available port
        config_.max_connections = 10;
        config_.receive_buffer_size = 4096;
        config_.connection_timeout = std::chrono::milliseconds(1000);
    }

    void TearDown() override
    {
        if (server_ && server_->is_running())
        {
            server_->stop();
        }
    }

    // Helper method to create and start server
    bool createAndStartServer()
    {
        server_ = std::make_unique<TCPServer>(config_);
        return server_->start();
    }

    // Helper method to wait for server to be ready
    bool waitForServerReady(int timeout_ms = 1000)
    {
        auto start_time = std::chrono::steady_clock::now();
        while (!server_->is_running())
        {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms)
            {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    TCPServer::Config config_;
    std::unique_ptr<TCPServer> server_;
    std::atomic<int> last_connected_client_id_{-1};
    std::atomic<int> last_disconnected_client_id_{-1};
    std::vector<uint8_t> last_received_data_;
    std::atomic<int> last_data_client_id_{-1};
};

// Test fixture for callback testing
class TCPServerCallbackTest : public TCPServerTestBase
{
protected:
    void SetUp() override
    {
        TCPServerTestBase::SetUp();

        // Set up mock callbacks
        mock_connected_ = std::make_unique<MockClientConnectedCallback>();
        mock_disconnected_ = std::make_unique<MockClientDisconnectedCallback>();
        mock_data_ = std::make_unique<MockClientDataCallback>();

        // Set default expectations (can be overridden in individual tests)
        EXPECT_CALL(*mock_connected_, operator())
            .WillRepeatedly([this](int client_id, const std::string&) {
                last_connected_client_id_.store(client_id);
            });

        EXPECT_CALL(*mock_disconnected_, operator())
            .WillRepeatedly([this](int client_id) {
                last_disconnected_client_id_.store(client_id);
            });

        EXPECT_CALL(*mock_data_, operator())
            .WillRepeatedly([this](int client_id, const std::vector<uint8_t>& data) {
                last_data_client_id_.store(client_id);
                last_received_data_ = data;
            });
    }

    void TearDown() override
    {
        if (server_ && server_->is_running())
        {
            server_->stop();
        }
    }

    // Helper to set callbacks on server
    void setServerCallbacks()
    {
        server_->set_client_connected_callback([this](int client_id, const std::string& address) {
            if (mock_connected_) {
                (*mock_connected_)(client_id, address);
            }
        });

        server_->set_client_disconnected_callback([this](int client_id) {
            if (mock_disconnected_) {
                (*mock_disconnected_)(client_id);
            }
        });

        server_->set_client_data_callback([this](int client_id, const std::vector<uint8_t>& data) {
            if (mock_data_) {
                (*mock_data_)(client_id, data);
            }
        });
    }

    std::unique_ptr<MockClientConnectedCallback> mock_connected_;
    std::unique_ptr<MockClientDisconnectedCallback> mock_disconnected_;
    std::unique_ptr<MockClientDataCallback> mock_data_;
};

// Test fixture for multi-client testing
class TCPServerMultiClientTest : public TCPServerCallbackTest
{
protected:
    void SetUp() override
    {
        TCPServerCallbackTest::SetUp();
        connected_clients_.clear();
        client_messages_.clear();
    }

    void TearDown() override
    {
        TCPServerCallbackTest::TearDown();
        connected_clients_.clear();
        client_messages_.clear();
    }

    // Track connected clients
    void onClientConnected(int client_id)
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_.insert(client_id);
    }

    void onClientDisconnected(int client_id)
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        connected_clients_.erase(client_id);
        client_messages_.erase(client_id);
    }

    void onClientData(int client_id, const std::vector<uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_messages_[client_id].push_back(data);
    }

    size_t getConnectedClientCount() const
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return connected_clients_.size();
    }

    const auto& getClientMessages(int client_id) const
    {
        static const std::vector<std::vector<uint8_t>> empty_messages;
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = client_messages_.find(client_id);
        return (it != client_messages_.end()) ? it->second : empty_messages;
    }

private:
    mutable std::mutex clients_mutex_;
    std::set<int> connected_clients_;
    std::map<int, std::vector<std::vector<uint8_t>>> client_messages_;
};

} // namespace exchange_simulator::test
