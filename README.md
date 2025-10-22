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

## Quick Start

### Prerequisites

- C++20 compatible compiler
- CMake 3.20 or higher

### Building

```bash
# Configure build
cmake -S . -B build

# Build project
cmake --build build --config Debug

# Run tests
cd build && ctest --output-on-failure -C Debug
```

### Windows (Visual Studio)

```bash
# Configure with Visual Studio generator
cmake -S . -B build -G "Visual Studio 17 2022"

# Build
cmake --build build --config Debug
```

## Usage

Since this is a header-only library, simply include the headers you need:

```cpp
#include "tcp_server.h"
#include "tcp_client.h" 
#include "multicast_sender.h"
#include "multicast_receiver.h"
```

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

## Build Options

### AddressSanitizer

Enable AddressSanitizer for debugging:

```bash
cmake -S . -B build -DENABLE_ASAN=ON
cmake --build build --config Debug
```

### Release Build

```bash
cmake --build build --config Release
```

## Architecture

The library uses a three-file pattern for cross-platform support:

- `component.h` - Base template class with platform-independent interface
- `component_win32.h` - Windows implementation
- `component_unix.h` - Unix/Linux implementation

This design uses CRTP (Curiously Recurring Template Pattern) for compile-time polymorphism without virtual function overhead.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.