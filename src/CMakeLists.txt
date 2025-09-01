
# Platform-specific implementations
if(WIN32)
    add_library(slick_socket INTERFACE tcp_server_win32.h tcp_server.h tcp_client_win32.h tcp_client.h multicast_sender_win32.h multicast_sender.h multicast_receiver_win32.h multicast_receiver.h logger.h)
    target_include_directories(slick_socket INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_compile_definitions(slick_socket INTERFACE _WIN32_WINNT=0x0601)
endif()

# Unix implementations for Linux/macOS
if(UNIX)
    add_library(slick_socket INTERFACE tcp_server_unix.h tcp_server.h tcp_client_unix.h tcp_client.h multicast_sender_unix.h multicast_sender.h multicast_receiver_unix.h multicast_receiver.h logger.h)
    target_include_directories(slick_socket INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

target_include_directories(slick_socket INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(WIN32)
    target_link_libraries(slick_socket INTERFACE ws2_32)
elseif(UNIX)
    # Unix doesn't need additional libraries for basic socket operations
endif()
