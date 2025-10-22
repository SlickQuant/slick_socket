# Tests CMakeLists.txt

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

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
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Discover tests with increased timeout
gtest_discover_tests(tests
    DISCOVERY_TIMEOUT 30
)

# Set test properties
set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
