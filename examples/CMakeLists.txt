add_executable(tcp_server_example
    tcp_server_example.cpp
)

target_include_directories(tcp_server_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tcp_server_example PRIVATE slick::socket)

set_target_properties(tcp_server_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(tcp_client_example
    tcp_client_example.cpp
)

target_include_directories(tcp_client_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(tcp_client_example PRIVATE slick::socket)

set_target_properties(tcp_client_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(multicast_sender_example
    multicast_sender_example.cpp
)

target_include_directories(multicast_sender_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(multicast_sender_example
    PRIVATE
    slick::socket)

set_target_properties(multicast_sender_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(multicast_receiver_example
    multicast_receiver_example.cpp
)

target_include_directories(multicast_receiver_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(multicast_receiver_example
    PRIVATE
    slick::socket)

set_target_properties(multicast_receiver_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

add_executable(multicast_integration_example
    multicast_integration_example.cpp
)

target_include_directories(multicast_integration_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(multicast_integration_example
    PRIVATE
    slick::socket)

set_target_properties(multicast_integration_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)
