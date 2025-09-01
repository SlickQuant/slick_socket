#include <multicast_sender.h>
#include <multicast_receiver.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

class MulticastReceiver : public slick_socket::MulticastReceiverBase<MulticastReceiver>
{
public:
    MulticastReceiver(const slick_socket::MulticastReceiverConfig& config)
        : slick_socket::MulticastReceiverBase<MulticastReceiver>("IntegrationReceiver", config)
    {
    }

    void handle_multicast_data(const std::vector<uint8_t>& data, const std::string& sender_address)
    {
        std::string message(data.begin(), data.end());
        std::cout << "  [RECEIVER] Got message from " << sender_address << ": " << message << std::endl;
        messages_received_++;
    }

    std::atomic<int> messages_received_{0};
};

class MulticastSender : public slick_socket::MulticastSenderBase<MulticastSender>
{
public:
    MulticastSender(const slick_socket::MulticastSenderConfig& config)
        : slick_socket::MulticastSenderBase<MulticastSender>("IntegrationSender", config)
    {
    }
};

int main()
{
    std::cout << "=== Multicast Integration Demo ===" << std::endl;
    std::cout << "This demo shows a sender and receiver working together." << std::endl;

    // Configure multicast
    const std::string multicast_address = "224.0.0.102";
    const uint16_t port = 12347;

    // Setup receiver
    slick_socket::MulticastReceiverConfig receiver_config;
    receiver_config.multicast_address = multicast_address;
    receiver_config.port = port;
    receiver_config.reuse_address = true;
    receiver_config.receive_timeout = std::chrono::milliseconds(1000);

    // Setup sender  
    slick_socket::MulticastSenderConfig sender_config;
    sender_config.multicast_address = multicast_address;
    sender_config.port = port;
    sender_config.ttl = 1; // Local network only
    sender_config.enable_loopback = true; // Enable loopback so we can receive our own messages

    // Create receiver and sender
    MulticastReceiver receiver(receiver_config);
    MulticastSender sender(sender_config);

    std::cout << "\n1. Starting receiver..." << std::endl;
    if (!receiver.start())
    {
        std::cerr << "Failed to start receiver!" << std::endl;
        return -1;
    }

    std::cout << "2. Starting sender..." << std::endl;
    if (!sender.start())
    {
        std::cerr << "Failed to start sender!" << std::endl;
        receiver.stop();
        return -1;
    }

    std::cout << "3. Both started successfully. Beginning message exchange..." << std::endl;
    
    // Give receiver time to join multicast group
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Send test messages
    const int num_messages = 5;
    for (int i = 1; i <= num_messages; ++i)
    {
        std::string message = "Integration test message #" + std::to_string(i);
        std::cout << "  [SENDER] Sending: " << message << std::endl;
        
        if (!sender.send_data(message))
        {
            std::cerr << "  [SENDER] Failed to send message!" << std::endl;
        }
        
        // Wait between sends
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "\n4. Waiting for final messages to arrive..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Display statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Sender Statistics:" << std::endl;
    std::cout << "  Packets sent: " << sender.get_packets_sent() << std::endl;
    std::cout << "  Bytes sent: " << sender.get_bytes_sent() << std::endl;
    std::cout << "  Send errors: " << sender.get_send_errors() << std::endl;

    std::cout << "\nReceiver Statistics:" << std::endl;
    std::cout << "  Messages received (custom): " << receiver.messages_received_.load() << std::endl;
    std::cout << "  Packets received: " << receiver.get_packets_received() << std::endl;
    std::cout << "  Bytes received: " << receiver.get_bytes_received() << std::endl;
    std::cout << "  Receive errors: " << receiver.get_receive_errors() << std::endl;

    // Cleanup
    std::cout << "\n5. Stopping sender and receiver..." << std::endl;
    sender.stop();
    receiver.stop();

    std::cout << "=== Demo Complete ===" << std::endl;

    // Verify success
    if (receiver.messages_received_.load() > 0)
    {
        std::cout << "SUCCESS: Multicast communication working!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "NOTE: No messages received. This might be normal depending on network configuration." << std::endl;
        return 0;
    }
}