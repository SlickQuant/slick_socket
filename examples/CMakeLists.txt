add_executable(tcp_server_example
    tcp_server_example.cpp
)

target_include_directories(tcp_server_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tcp_client_example
    tcp_client_example.cpp
)

target_include_directories(tcp_client_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(multicast_sender_example
    multicast_sender_example.cpp
)

target_include_directories(multicast_sender_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(multicast_sender_example
    PRIVATE
    slick_socket)
