cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(slick_socket
  VERSION 1.0.2
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
  add_library(slick_socket STATIC src/wepoll.c)
  add_library(slick::slick_socket ALIAS slick_socket)
  set_target_properties(slick_socket PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  )
  target_include_directories(slick_socket PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(slick_socket PUBLIC _WIN32_WINNT=0x0601)
  target_link_libraries(slick_socket PUBLIC ws2_32)

  # Shared lib
  add_library(slick_socket_shared SHARED src/wepoll.c)
  add_library(slick::slick_socket_shared ALIAS slick_socket_shared)
  set_target_properties(slick_socket_shared PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  )
  target_include_directories(slick_socket_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_compile_definitions(slick_socket_shared PUBLIC _WIN32_WINNT=0x0601)
  target_link_libraries(slick_socket_shared PUBLIC ws2_32)

elseif(UNIX)
  add_library(slick_socket INTERFACE)
  target_include_directories(slick_socket INTERFACE
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

# Installation rules
install(DIRECTORY include/ DESTINATION include)
if (WIN32)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/ DESTINATION lib)
endif()

# Automatically run install after build in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if (WIN32)
    add_custom_target(dist_slick_socket ALL
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
      COMMENT "Copying slick_socket headers to dist/include"
      VERBATIM
    )
    add_dependencies(dist_slick_socket slick_socket)

    add_custom_command(TARGET dist_slick_socket POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/lib
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/bin
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:slick_socket> ${CMAKE_BINARY_DIR}/dist/lib/
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:slick_socket_shared> ${CMAKE_BINARY_DIR}/dist/bin/
      COMMENT "Copying slick_socket.lib to dist/lib; Copying slick_socket.dll to dist/bin"
    )
  else()
    add_custom_target(dist_slick_socket ALL
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/dist/include
      COMMENT "Copying slick_socket headers to dist/include"
      VERBATIM
    )
  endif()

  if (PROJECT_IS_TOP_LEVEL)
    if (WIN32)
      add_custom_target(package_slick_socket ALL
        COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_socket_${PROJECT_VERSION}.zip" --format=zip "include" "lib" "bin"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
        COMMENT "Creating zip archive"
      )
    else()
      add_custom_target(package_slick_socket ALL
        COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/dist/slick_socket_${PROJECT_VERSION}.zip" --format=zip "include"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
        COMMENT "Creating zip archive"
      )
    endif()

    add_dependencies(package_slick_socket dist_slick_socket)
  endif()
endif()
