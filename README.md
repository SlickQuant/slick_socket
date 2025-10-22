# slick_socket

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![CI](https://github.com/SlickQuant/slick_socket/actions/workflows/ci.yml/badge.svg)](https://github.com/SlickQuant/slick_socket/actions/workflows/ci.yml)
[![GitHub release](https://img.shields.io/github/v/release/SlickQuant/slick_socket)](https://github.com/SlickQuant/slick_socket/releases)

A header-only C++20 networking library providing cross-platform TCP and UDP multicast communication.

## Features

- **Cross-platform**: Windows and Unix/Linux support
- **Header-only**: No separate compilation required
- **Modern C++**: C++20 template-based design with CRTP
- **Asynchronous**: Non-blocking socket operations with timeout handling
- **TCP Communication**: Client and server implementations
- **UDP Multicast**: One-to-many communication support
- **Logging**: Template-based logger interface with console output

## Installation

### Using FetchContent (Recommended)

The easiest way to use slick_socket is to fetch it directly in your CMakeLists.txt:

```cmake
include(FetchContent)

FetchContent_Declare(
    slick_socket
    GIT_REPOSITORY https://github.com/SlickQuant/slick_socket.git
    GIT_TAG v1.0.0.1  # Use the desired version
)

FetchContent_MakeAvailable(slick_socket)

# Link against slick_socket
target_link_libraries(your_target PRIVATE slick_socket)
```

On Windows, you also need to link against `ws2_32`:
```cmake
target_link_libraries(your_target PRIVATE slick_socket ws2_32)
```

### From Source

#### Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 12+, MSVC 2022+)
- CMake 3.25 or higher

#### Unix/Linux/macOS

1. **Configure the build**:
   ```bash
   cmake -S . -B build
   ```

2. **Build the library**:
   ```bash
   cmake --build build --config Release
   ```

3. **Copy to your project**:
   ```bash
   cp -r build/dist/include/slick /path/to/your/project/include/
   ```

   The library is header-only on Unix/Linux/macOS platforms, so only headers are needed.

#### Windows (Visual Studio)

1. **Configure the build**:
   ```bash
   cmake -S . -B build -G "Visual Studio 17 2022"
   ```

2. **Build the library**:
   ```bash
   cmake --build build --config Release
   ```

3. **Copy headers and library**:
   ```bash
   xcopy build\dist\include\slick <your-project>\include\slick /E
   xcopy build\dist\lib\slick_socket.lib <your-project>\lib\
   ```

4. **Link in your CMakeLists.txt**:
   ```cmake
   # Add the slick_socket include and lib directories
   target_include_directories(your_target PRIVATE path/to/slick/include)
   target_link_directories(your_target PRIVATE path/to/slick/lib)

   # Link the libraries
   target_link_libraries(your_target PRIVATE slick_socket ws2_32)
   ```

   **Or manually** in Visual Studio:
   - Add include path: `path/to/slick/include`
   - Add library path: `path/to/slick/lib`
   - Link with: `slick_socket.lib` and `ws2_32.lib`

## Usage

### Basic Example

Include the headers you need in your project:

```cpp
#include <slick/socket/tcp_server.h>
#include <slick/socket/tcp_client.h>
#include <slick/socket/multicast_sender.h>
#include <slick/socket/multicast_receiver.h>
```

### Creating a TCP Server

```cpp
#include <slick/socket/tcp_server.h>

class MyServer : public slick::socket::TCPServerBase<MyServer>
{
public:
    MyServer() : TCPServerBase("MyServer", {/*.port = 5000*/}) {}

    void onClientConnected(int client_id, const std::string& address)
    {
        std::cout << "Client " << client_id << " connected from " << address << std::endl;
    }

    void onClientData(int client_id, const uint8_t* data, size_t size)
    {
        // Echo back to client
        send_data(client_id, std::vector<uint8_t>(data, data + size));
    }

    void onClientDisconnected(int client_id)
    {
        std::cout << "Client " << client_id << " disconnected" << std::endl;
    }
};

int main()
{
    MyServer server;
    server.start();
    // ... server runs in background thread
    return 0;
}
```

For more examples, see the [examples/](examples/) directory.

## Testing

Run the complete test suite:

```bash
cd build && ctest --output-on-failure -C Debug
```

Run specific tests:

```bash
cd build && ctest -R TCPServerTest -C Debug
```

Enable verbose test output:

```bash
cd build && ctest -V -C Debug
```

## Development

### Build Options

#### AddressSanitizer

Enable AddressSanitizer for debugging memory issues:

```bash
cmake -S . -B build -DENABLE_ASAN=ON
cmake --build build --config Debug
```

#### Release Build with Optimization

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Project Structure

```
slick_socket/
├── include/slick/socket/     # Public headers
│   ├── tcp_server.h          # TCP server base class
│   ├── tcp_client.h          # TCP client base class
│   ├── multicast_sender.h    # UDP multicast sender
│   ├── multicast_receiver.h  # UDP multicast receiver
│   └── logger.h              # Logger interface
├── src/                       # Implementation files (Windows-specific)
├── examples/                  # Usage examples
├── tests/                     # Unit and integration tests
└── CMakeLists.txt
```

## Architecture

The library uses a three-file pattern for cross-platform support:

- `component.h` - Base template class with platform-independent interface
- `component_win32.h` - Windows implementation
- `component_unix.h` - Unix/Linux implementation

This design uses CRTP (Curiously Recurring Template Pattern) for compile-time polymorphism without virtual function overhead.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Made with ⚡ by [SlickQuant](https://github.com/SlickQuant)**