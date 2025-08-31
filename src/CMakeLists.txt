# Network module CMakeLists.txt
add_library(socket INTERFACE)
target_include_directories(socket INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Platform-specific implementations
if(WIN32)
    add_library(socket_win32 INTERFACE tcp_server_win32.h tcp_server_base.h)
    target_include_directories(socket_win32 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_definitions(socket_win32 INTERFACE _WIN32_WINNT=0x0601)
endif()

# Unix implementations for Linux/macOS
if(UNIX)
    add_library(socket_unix INTERFACE tcp_server_unix.h tcp_server_base.h)
    target_include_directories(socket_unix INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# Main network interface
add_library(tcp_server INTERFACE)
target_link_libraries(tcp_server INTERFACE socket)

if(WIN32)
    target_link_libraries(tcp_server INTERFACE socket_win32 ws2_32)
elseif(UNIX)
    target_link_libraries(tcp_server INTERFACE socket_unix)
endif()
