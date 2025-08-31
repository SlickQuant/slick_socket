#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <tcp_server.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "mock_network.h"
#include "test_fixtures.h"

#pragma comment(lib, "ws2_32.lib")

namespace slick_socket::test
{

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;

class TCPServer : public TCPServerBase
{
public:
    using TCPServerBase::TCPServerBase;

    // Override event handlers to make them public for testing
    void onClientConnected(int client_id, const std::string& client_address)
    {
        std::cout << "Client connected: ID=" << client_id << ", Address=" << client_address << std::endl;
    }

    void onClinetDisconnected(int client_id)
    {
        std::cout << "Client disconnected: ID=" << client_id << std::endl;
    }

    void onClentData(int client_id, const std::vector<uint8_t>& data)
    {
        std::cout << "Data received from client ID=" << client_id << ", Size=" << data.size() << std::endl;
    }

    void logTrace(const std::string& format, auto&&... args)
    {
        std::cout << "[TRACE] " << std::vformat(format, std::forward<decltype(args)>(args...)) << std::endl;
    }

    void logDebug(const std::string& format, auto&&... args)
    {
        std::cout << "[DEBUG] " << std::vformat(format, std::forward<decltype(args)>(args...)) << std::endl;
    }

    void logInfo(const std::string& format, auto&&... args)
    {
        std::cout << "[INFO] " << std::vformat(format, std::forward<decltype(args)>(args...)) << std::endl;
    }

    void logWarning(const std::string& format, auto&&... args)
    {
        std::cout << "[WARN] " << std::vformat(format, std::forward<decltype(args)>(args...)) << std::endl;
    }

    void logError(const std::string& format, auto&&... args)
    {
        std::cout << "[ERROR] " << std::vformat(format, std::forward<decltype(args)>(args...)) << std::endl;
    }
}

// Test basic server lifecycle
TEST_F(TCPServerTestBase, ServerCreationAndDestruction)
{
    // Test server creation
    server_ = std::make_unique<TCPServer>(config_);
    ASSERT_NE(server_, nullptr);
    EXPECT_FALSE(server_->is_running());
}

TEST_F(TCPServerTestBase, ServerStartAndStop)
{
    server_ = std::make_unique<TCPServer>(config_);

    // Test server start
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(waitForServerReady());
    EXPECT_TRUE(server_->is_running());

    // Test server stop
    server_->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(server_->is_running());
}

TEST_F(TCPServerTestBase, ServerRestart)
{
    server_ = std::make_unique<TCPServer>(config_);

    // Start server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(waitForServerReady());
    EXPECT_TRUE(server_->is_running());

    // Stop server
    server_->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(server_->is_running());

    // Restart server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(waitForServerReady());
    EXPECT_TRUE(server_->is_running());
}

// Test callback functionality
TEST_F(TCPServerCallbackTest, ClientConnectedCallback)
{
    setServerCallbacks();

    // Set specific expectation for this test
    EXPECT_CALL(*mock_connected_, operator()(1, ::testing::StrEq("127.0.0.1")))
        .Times(1);

    ASSERT_TRUE(createAndStartServer());
    ASSERT_TRUE(waitForServerReady());

    // Simulate client connection (this would normally happen through network)
    // For now, we'll test the callback mechanism directly
    // In a real integration test, we'd create actual client connections

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(TCPServerCallbackTest, ClientDisconnectedCallback)
{
    setServerCallbacks();

    // Set specific expectation for this test
    EXPECT_CALL(*mock_disconnected_, operator()(1))
        .Times(1);

    ASSERT_TRUE(createAndStartServer());
    ASSERT_TRUE(waitForServerReady());

    // Test would involve connecting a client and then disconnecting
    // This is a framework test to ensure callbacks are properly set

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(TCPServerCallbackTest, ClientDataCallback)
{
    setServerCallbacks();

    std::vector<uint8_t> test_data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"

    // Set specific expectation for this test
    EXPECT_CALL(*mock_data_, operator()(1, test_data))
        .Times(1);

    ASSERT_TRUE(createAndStartServer());
    ASSERT_TRUE(waitForServerReady());

    // Test would involve sending data through a connected client
    // This verifies the callback mechanism is properly configured

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Test configuration validation
TEST_F(TCPServerTestBase, ConfigurationValidation)
{
    // Test valid configuration
    {
        TCPServer::Config valid_config;
        valid_config.port = 8080;
        valid_config.max_connections = 100;
        valid_config.receive_buffer_size = 8192;
        valid_config.connection_timeout = std::chrono::milliseconds(5000);

        server_ = std::make_unique<TCPServer>(valid_config);
        EXPECT_TRUE(server_->start());
        server_->stop();
    }

    // Test extreme values
    {
        TCPServer::Config extreme_config;
        extreme_config.port = 65535;
        extreme_config.max_connections = 1;
        extreme_config.receive_buffer_size = 1024;
        extreme_config.connection_timeout = std::chrono::milliseconds(10);

        server_ = std::make_unique<TCPServer>(extreme_config);
        EXPECT_TRUE(server_->start());
        server_->stop();
    }
}

// Test server statistics
TEST_F(TCPServerTestBase, ServerStatistics)
{
    server_ = std::make_unique<TCPServer>(config_);
    ASSERT_TRUE(server_->start());
    ASSERT_TRUE(waitForServerReady());

    // Test initial state
    EXPECT_EQ(server_->get_connected_client_count(), 0u);

    // Test that statistics don't crash the server
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(server_->get_connected_client_count(), 0u);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    server_->stop();
}

// Test concurrent access
TEST_F(TCPServerTestBase, ConcurrentAccess)
{
    server_ = std::make_unique<TCPServer>(config_);
    ASSERT_TRUE(server_->start());
    ASSERT_TRUE(waitForServerReady());

    std::atomic<bool> stop_threads{false};
    std::vector<std::thread> threads;

    // Start multiple threads accessing server
    for (int i = 0; i < 5; ++i)
    {
        threads.emplace_back([this, &stop_threads]() {
            while (!stop_threads.load())
            {
                server_->is_running();
                server_->get_connected_client_count();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop threads
    stop_threads.store(true);
    for (auto& thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    server_->stop();
}

// Test error handling
TEST_F(TCPServerTestBase, ErrorHandling)
{
    // Test creating server with invalid configuration
    {
        TCPServer::Config invalid_config;
        invalid_config.port = 99999; // Invalid port
        invalid_config.max_connections = -1; // Invalid count

        server_ = std::make_unique<TCPServer>(invalid_config);
        // Server should still be created, but may fail to start
        // This tests that invalid config doesn't crash construction
    }

    // Test operations on stopped server
    {
        server_ = std::make_unique<TCPServer>(config_);

        // These operations should not crash even if server is not started
        EXPECT_FALSE(server_->is_running());
        EXPECT_EQ(server_->get_connected_client_count(), 0u);

        // Stop an already stopped server should be safe
        server_->stop();
        EXPECT_FALSE(server_->is_running());
    }
}

// Test resource management
TEST_F(TCPServerTestBase, ResourceManagement)
{
    // Test multiple server instances
    std::vector<std::unique_ptr<TCPServer>> servers;

    for (int i = 0; i < 3; ++i)
    {
        auto server = std::make_unique<TCPServer>(config_);
        servers.push_back(std::move(server));
    }

    // Start all servers
    for (auto& server : servers)
    {
        EXPECT_TRUE(server->start());
    }

    // Stop all servers
    for (auto& server : servers)
    {
        server->stop();
    }

    // Clear servers (test destruction)
    servers.clear();
}

// Integration test for full server lifecycle
TEST_F(TCPServerCallbackTest, FullLifecycleTest)
{
    setServerCallbacks();

    // Test complete server lifecycle
    ASSERT_TRUE(createAndStartServer());
    ASSERT_TRUE(waitForServerReady());

    // Server should be running and accepting connections
    EXPECT_TRUE(server_->is_running());
    EXPECT_EQ(server_->get_connected_client_count(), 0u);

    // Test server can be stopped and restarted
    server_->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(server_->is_running());

    // Restart server
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(waitForServerReady());
    EXPECT_TRUE(server_->is_running());

    // Final cleanup
    server_->stop();
}

} // namespace exchange_simulator::test
