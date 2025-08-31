add_executable(tcp_server_example
    tcp_server_example.cpp
)

target_include_directories(tcp_server_example
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})
