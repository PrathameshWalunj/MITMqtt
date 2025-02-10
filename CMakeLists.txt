cmake_minimum_required(VERSION 3.15)
project(MITMqtt VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MITMQTT_BUILD_TESTS "Build test suite" OFF)
option(MITMQTT_ENABLE_WARNINGS "Enable warnings" ON)

# Dependencies
include(FetchContent)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_MakeAvailable(imgui)

# GLFW for window management
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Mosquitto MQTT library
FetchContent_Declare(
    mosquitto
    GIT_REPOSITORY https://github.com/eclipse/mosquitto.git
    GIT_TAG v2.0.15
)
FetchContent_MakeAvailable(mosquitto)

# OpenSSL for TLS support
find_package(OpenSSL REQUIRED)

# Set compiler warnings
if(MITMQTT_ENABLE_WARNINGS)
    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

# Add subdirectories
add_subdirectory(src)
if(MITMQTT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
install(DIRECTORY include/MITMqtt
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
)
