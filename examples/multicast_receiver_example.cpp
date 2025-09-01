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
        : slick_socket::MulticastReceiverBase<MulticastReceiver>("MulticastReceiver", config)
    {
    }

    void handle_multicast_data(const std::vector<uint8_t>& data, const std::string& sender_address)
    {
        std::string message(data.begin(), data.end());
        std::cout << "Received from " << sender_address << ": " << message << std::endl;
        
        // Update statistics
        messages_received_++;
    }

    std::atomic<int> messages_received_{0};
};

int main()
{
    slick_socket::MulticastReceiverConfig config;
    config.multicast_address = "224.0.0.100"; // Same as sender example
    config.port = 12345;
    config.reuse_address = true; // Allow multiple receivers
    config.receive_timeout = std::chrono::milliseconds(1000);

    MulticastReceiver receiver(config);

    std::cout << "Starting multicast receiver..." << std::endl;
    if (!receiver.start())
    {
        std::cerr << "Failed to start multicast receiver." << std::endl;
        return -1;
    }

    std::cout << "Multicast receiver started. Listening for messages on " 
              << config.multicast_address << ":" << config.port << std::endl;
    std::cout << "Press Ctrl+C to stop or wait 30 seconds..." << std::endl;

    // Run for 30 seconds or until interrupted
    auto start_time = std::chrono::steady_clock::now();
    auto max_duration = std::chrono::seconds(30);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= max_duration)
        {
            break;
        }

        // Show periodic status
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() % 5 == 0)
        {
            std::cout << "Still listening... (received " << receiver.messages_received_.load() 
                      << " messages so far)" << std::endl;
        }
    }

    // Display final statistics
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Messages received (custom): " << receiver.messages_received_.load() << std::endl;
    std::cout << "  Packets received: " << receiver.get_packets_received() << std::endl;
    std::cout << "  Bytes received: " << receiver.get_bytes_received() << std::endl;
    std::cout << "  Receive errors: " << receiver.get_receive_errors() << std::endl;

    std::cout << "Stopping multicast receiver..." << std::endl;
    receiver.stop();
    std::cout << "Multicast receiver stopped." << std::endl;

    return 0;
}