cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(slick-socket
  VERSION 1.0.4
  LANGUAGES C CXX
)

# MSVC configuration
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
  add_definitions(-DNDEBUG)
endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
  if(MSVC)
    # MSVC AddressSanitizer (requires Visual Studio 2022 17.7+)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.37")
      add_compile_options(/fsanitize=address)
      add_link_options(/INCREMENTAL:NO)

      # Additional MSVC ASan flags for better compatibility
      add_compile_definitions(_DISABLE_VECTOR_ANNOTATION)
      add_compile_definitions(_DISABLE_STRING_ANNOTATION)

      message(STATUS "AddressSanitizer enabled for MSVC ${CMAKE_CXX_COMPILER_VERSION}")
    else()
      message(WARNING "AddressSanitizer requires Visual Studio 2022 17.7+ (current: ${CMAKE_CXX_COMPILER_VERSION})")
      set(ENABLE_ASAN OFF CACHE BOOL "ASan disabled - compiler too old" FORCE)
    endif()
  else()
    # GCC/Clang AddressSanitizer
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

if (MSVC)
  set(CMAKE_SUPPRESS_REGENERATION true)   # supress zero_check project
endif()

if(WIN32)
  # Static lib
  option(BUILD_SLICK_SOCKET_STATIC_LIBS "Build tests" ON)
  if (BUILD_SLICK_SOCKET_STATIC_LIBS)
    add_library(slick-socket STATIC src/wepoll.c)
    add_library(slick::socket ALIAS slick-socket)
    # For backward compatibility
    add_library(slick_socket ALIAS slick-socket)
    add_library(slick::slick_socket ALIAS slick-socket)

    set_target_properties(slick-socket PROPERTIES 
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )
    target_include_directories(slick-socket PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(slick-socket PUBLIC _WIN32_WINNT=0x0601)
    target_link_libraries(slick-socket PUBLIC ws2_32)
  endif()

  # Shared lib
  option(BUILD_SLICK_SOCKET_SHARED_LIBS "Build tests" ON)
  if (BUILD_SLICK_SOCKET_SHARED_LIBS)
    add_library(slick-socket-shared SHARED src/wepoll.c)
    add_library(slick::socket-shared ALIAS slick-socket-shared)
    add_library(slick_socket_shared ALIAS slick-socket-shared)
    add_library(slick::slick_socket_shared ALIAS slick-socket-shared)

    set_target_properties(slick-socket-shared PROPERTIES 
      ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    target_include_directories(slick-socket-shared PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(slick-socket-shared PUBLIC _WIN32_WINNT=0x0601)
    target_link_libraries(slick-socket-shared PUBLIC ws2_32)
  endif()

elseif(UNIX)
  add_library(slick-socket INTERFACE)
  target_include_directories(slick-socket INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
endif()

option(BUILD_SLICK_SOCKET_EXAMPLES "Build tests" ON)
if (BUILD_SLICK_SOCKET_EXAMPLES)
  add_subdirectory(examples)
endif()

# Tests
option(BUILD_SLICK_SOCKET_TESTING "Build tests" ON)
if(BUILD_SLICK_SOCKET_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

install(DIRECTORY include/ DESTINATION include)
if (WIN32)
  if (BUILD_SLICK_SOCKET_STATIC_LIBS)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/ DESTINATION lib)
  endif()
  if (BUILD_SLICK_SOCKET_SHARED_LIBS)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}/ DESTINATION bin)
  endif()
endif()
