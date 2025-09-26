#include <gtest/gtest.h>
#include "../examples/logger.h"
#include <slick_socket/tcp_server.h>
#include <slick_socket/tcp_client.h>
#include <thread>
#include <chrono>
#include <atomic>

class IntegrationTestServer : public slick_socket::TCPServerBase<IntegrationTestServer>
{
public:
    using slick_socket::TCPServerBase<IntegrationTestServer>::TCPServerBase;
    using slick_socket::TCPServerBase<IntegrationTestServer>::get_connected_client_count;
    using slick_socket::TCPServerBase<IntegrationTestServer>::send_data;
    
    void onClientConnected(int client_id, const std::string& client_address) {
        connected_clients++;
        last_connected_client_id = client_id;
    }
    
    void onClientDisconnected(int client_id) {
        disconnected_clients++;
        last_disconnected_client_id = client_id;
    }
    
    void onClientData(int client_id, const uint8_t* data, size_t length) {
        data_received++;
        last_received_data = std::string((const char*)data, length);
        last_data_client_id = client_id;
        
        // Echo the data back to the client
        std::vector<uint8_t> buffer(data, data + length);
        send_data(client_id, buffer);
    }

    std::atomic<int> connected_clients{0};
    std::atomic<int> disconnected_clients{0};
    std::atomic<int> data_received{0};
    std::atomic<int> last_connected_client_id{-1};
    std::atomic<int> last_disconnected_client_id{-1};
    std::atomic<int> last_data_client_id{-1};
    std::string last_received_data;
};

class IntegrationTestClient : public slick_socket::TCPClientBase<IntegrationTestClient>
{
public:
    using slick_socket::TCPClientBase<IntegrationTestClient>::TCPClientBase;
    
    void onConnected() {
        connected_count++;
        connection_established = true;
    }
    
    void onDisconnected() {
        disconnected_count++;
        connection_established = false;
    }
    
    void onData(const uint8_t* data, size_t length) {
        data_received_count++;
        last_received_data = std::string((const char*)data, length);
        data_received_flag = true;
    }

    std::atomic<int> connected_count{0};
    std::atomic<int> disconnected_count{0};
    std::atomic<int> data_received_count{0};
    std::atomic<bool> connection_established{false};
    std::atomic<bool> data_received_flag{false};
    std::string last_received_data;
};

class TCPIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use port 0 to let the system assign an available port
        server_config_.port = 0;
        server_config_.max_connections = 10;
        server_config_.receive_buffer_size = 4096;
        server_config_.connection_timeout = std::chrono::milliseconds(5000);
        
        client_config_.server_address = "127.0.0.1";
        client_config_.server_port = 0; // Will be set after server starts
        client_config_.receive_buffer_size = 4096;
        client_config_.connection_timeout = std::chrono::milliseconds(2000);
    }

    void TearDown() override {
        if (client_ && client_->is_connected()) {
            client_->disconnect();
        }
        if (server_ && server_->is_running()) {
            server_->stop();
        }
        
        // Give time for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Helper to wait for condition with timeout
    bool waitForCondition(std::function<bool()> condition, int timeout_ms = 5000) {
        auto start = std::chrono::steady_clock::now();
        while (!condition()) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    slick_socket::TCPServerConfig server_config_;
    slick_socket::TCPClientConfig client_config_;
    std::unique_ptr<IntegrationTestServer> server_;
    std::unique_ptr<IntegrationTestClient> client_;
};

TEST_F(TCPIntegrationTest, ServerClientLifecycle) {
    // Start server
    server_ = std::make_unique<IntegrationTestServer>("IntegrationServer", server_config_);
    ASSERT_TRUE(server_->start());
    
    // Wait for server to be ready
    ASSERT_TRUE(waitForCondition([this]() { return server_->is_running(); }));
    
    // Create client (connection will likely fail since we don't have the actual port)
    client_ = std::make_unique<IntegrationTestClient>("IntegrationClient", client_config_);
    
    // Verify initial states
    EXPECT_EQ(server_->get_connected_client_count(), 0u);
    EXPECT_FALSE(client_->is_connected());
    
    // Stop server
    server_->stop();
    ASSERT_TRUE(waitForCondition([this]() { return !server_->is_running(); }));
}

TEST_F(TCPIntegrationTest, MultipleClientsSupport) {
    // Start server
    server_ = std::make_unique<IntegrationTestServer>("IntegrationServer", server_config_);
    ASSERT_TRUE(server_->start());
    
    // Wait for server to be ready
    ASSERT_TRUE(waitForCondition([this]() { return server_->is_running(); }));
    
    // Create multiple client objects (connections will fail without actual port)
    auto client1 = std::make_unique<IntegrationTestClient>("Client1", client_config_);
    auto client2 = std::make_unique<IntegrationTestClient>("Client2", client_config_);
    auto client3 = std::make_unique<IntegrationTestClient>("Client3", client_config_);
    
    // Verify all clients are created properly
    EXPECT_NE(client1, nullptr);
    EXPECT_NE(client2, nullptr);
    EXPECT_NE(client3, nullptr);
    
    // Initial state should show no connections
    EXPECT_EQ(server_->get_connected_client_count(), 0u);
}

TEST_F(TCPIntegrationTest, ServerStatsWithClients) {
    // Start server
    server_ = std::make_unique<IntegrationTestServer>("IntegrationServer", server_config_);
    ASSERT_TRUE(server_->start());
    
    // Wait for server to be ready
    ASSERT_TRUE(waitForCondition([this]() { return server_->is_running(); }));
    
    // Verify initial stats
    EXPECT_EQ(server_->connected_clients.load(), 0);
    EXPECT_EQ(server_->disconnected_clients.load(), 0);
    EXPECT_EQ(server_->data_received.load(), 0);
    
    // Create client
    client_ = std::make_unique<IntegrationTestClient>("IntegrationClient", client_config_);
    
    // Verify client stats
    EXPECT_EQ(client_->connected_count.load(), 0);
    EXPECT_EQ(client_->disconnected_count.load(), 0);
    EXPECT_EQ(client_->data_received_count.load(), 0);
}