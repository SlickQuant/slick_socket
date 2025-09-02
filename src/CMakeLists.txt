
# Platform-specific implementations
if(WIN32)
    add_library(slick_socket INTERFACE
        ../include/slick_socket/tcp_server_win32.h
        ../include/slick_socket/tcp_server.h
        ../include/slick_socket/tcp_client_win32.h
        ../include/slick_socket/tcp_client.h
        ../include/slick_socket/multicast_sender_win32.h
        ../include/slick_socket/multicast_sender.h
        ../include/slick_socket/multicast_receiver_win32.h
        ../include/slick_socket/multicast_receiver.h
        ../include/slick_socket/logger.h
    )
    target_compile_definitions(slick_socket INTERFACE _WIN32_WINNT=0x0601)
endif()

# Unix implementations for Linux/macOS
if(UNIX)
    add_library(slick_socket INTERFACE
        ../include/slick_socket/tcp_server_unix.h
        ../include/slick_socket/tcp_server.h
        ../include/slick_socket/tcp_client_unix.h
        ../include/slick_socket/tcp_client.h multicast_sender_unix.h
        ../include/slick_socket/multicast_sender.h
        ../include/slick_socket/multicast_receiver_unix.h
        ../include/slick_socket/multicast_receiver.h 
        ../include/slick_socket/logger.h
    )
endif()

target_include_directories(slick_socket INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../inlcude)

if(WIN32)
    target_link_libraries(slick_socket INTERFACE ws2_32)
elseif(UNIX)
    # Unix doesn't need additional libraries for basic socket operations
endif()
