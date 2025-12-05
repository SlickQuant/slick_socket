#include "logger.h"
#include <slick/socket/multicast_sender.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main()
{
    slick::socket::MulticastSenderConfig config;
    config.multicast_address = "224.0.0.100"; // Test multicast address
    config.port = 12345;
    config.ttl = 1; // Local network only
    config.enable_loopback = false;

    slick::socket::MulticastSender sender("MulticastSender", config);

    std::cout << "Starting multicast sender..." << std::endl;
    if (!sender.start())
    {
        std::cerr << "Failed to start multicast sender." << std::endl;
        return -1;
    }

    std::cout << "Multicast sender started. Sending messages to " 
              << config.multicast_address << ":" << config.port << std::endl;

    // Send some test messages
    for (int i = 0; i < 10; ++i)
    {
        std::string message = "Hello Multicast World! Message #" + std::to_string(i + 1);
        
        if (sender.send_data(message))
        {
            std::cout << "Sent: " << message << std::endl;
        }
        else
        {
            std::cerr << "Failed to send message: " << message << std::endl;
        }

        // Wait between sends
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Display statistics
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Packets sent: " << sender.get_packets_sent() << std::endl;
    std::cout << "  Bytes sent: " << sender.get_bytes_sent() << std::endl;
    std::cout << "  Send errors: " << sender.get_send_errors() << std::endl;

    std::cout << "Stopping multicast sender..." << std::endl;
    sender.stop();
    std::cout << "Multicast sender stopped." << std::endl;

    return 0;
}