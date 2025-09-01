# Tests CMakeLists.txt

# Include Google Test
include(GoogleTest)

# Network Tests
add_executable(tests
    tcp_server_tests.cpp
    tcp_client_tests.cpp
    integration_tests.cpp
    multicast_sender_tests.cpp
    multicast_receiver_tests.cpp
)

target_link_libraries(tests
    PRIVATE
    GTest::gtest_main
    slick_socket
)

target_include_directories(tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Discover tests
gtest_discover_tests(tests)

# Set test properties
set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
