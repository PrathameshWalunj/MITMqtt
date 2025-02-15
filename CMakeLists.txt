cmake_minimum_required(VERSION 3.15)
project(MITMqtt VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MITMQTT_BUILD_TESTS "Build test suite" OFF)
option(MITMQTT_ENABLE_WARNINGS "Enable warnings" ON)

# Find Boost
set(BOOST_ROOT "C:/Boost/boost_1_87_0/boost_1_87_0")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.82.0 REQUIRED)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find Boost!")
endif()

# Log Boost information for debugging
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")

# Find other required packages
find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)

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

# MQTT Library (using Boost.Beast WebSocket)
include_directories(${Boost_INCLUDE_DIRS})
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_ALL_DYN_LINK)

# nlohmann-json (for packet analysis)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

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