#pragma once

#include <gmock/gmock.h>
#include <tcp_server.h>
#include <vector>
#include <string>

namespace slick_socket::test
{

// Mock callback classes for testing TCPServer callbacks
class MockClientConnectedCallback
{
public:
    MOCK_METHOD2(Call, void(int client_id, const std::string& client_address));
    void operator()(int client_id, const std::string& client_address) { Call(client_id, client_address); }
};

class MockClientDisconnectedCallback
{
public:
    MOCK_METHOD1(Call, void(int client_id));
    void operator()(int client_id) { Call(client_id); }
};

class MockClientDataCallback
{
public:
    MOCK_METHOD2(Call, void(int client_id, const std::vector<uint8_t>& data));
    void operator()(int client_id, const std::vector<uint8_t>& data) { Call(client_id, data); }
};

// Mock network interface for testing network operations
class MockNetworkInterface
{
public:
    MOCK_METHOD0(initialize, bool());
    MOCK_METHOD1(createSocket, bool(SOCKET& socket));
    MOCK_METHOD2(bindSocket, bool(SOCKET socket, uint16_t port));
    MOCK_METHOD2(listenSocket, bool(SOCKET socket, int backlog));
    MOCK_METHOD3(acceptConnection, bool(SOCKET server_socket, SOCKET& client_socket, std::string& client_address));
    MOCK_METHOD2(receiveData, bool(SOCKET socket, std::vector<uint8_t>& buffer));
    MOCK_METHOD2(sendData, bool(SOCKET socket, const std::vector<uint8_t>& data));
    MOCK_METHOD1(closeSocket, void(SOCKET socket));
    MOCK_METHOD0(cleanup, void());
};

// Test helper class for simulating client connections
class MockClient
{
public:
    MockClient();
    ~MockClient();

    bool connect(const std::string& host, uint16_t port);
    bool sendData(const std::vector<uint8_t>& data);
    bool sendData(const std::string& data);
    bool receiveData(std::vector<uint8_t>& buffer);
    void disconnect();

    bool isConnected() const { return connected_; }
    int getClientId() const { return client_id_; }

private:
    bool connected_;
    int client_id_;
    std::string server_address_;
    uint16_t server_port_;
};

// Test utilities for network testing
class NetworkTestUtils
{
public:
    static std::vector<uint8_t> createTestData(size_t size);
    static std::string createTestString(size_t length);
    static bool waitForCondition(std::function<bool()> condition, int timeout_ms = 5000);
    static void simulateNetworkDelay(int milliseconds);
};

} // namespace slick_socket::test
