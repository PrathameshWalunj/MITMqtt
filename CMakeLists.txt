cmake_minimum_required(VERSION 3.20)

project(MITMqtt
    VERSION 0.1.0
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MITMQTT_BUILD_TESTS "Build test suite" OFF)
option(MITMQTT_ENABLE_WARNINGS "Enable warnings" ON)

# Dependencies
# Boost - set hints for macOS Homebrew
if(APPLE)
    if(EXISTS "/opt/homebrew/opt/boost")
        set(BOOST_ROOT "/opt/homebrew/opt/boost")
        set(Boost_INCLUDE_DIR "/opt/homebrew/opt/boost/include")
    elseif(EXISTS "/usr/local/opt/boost")
        set(BOOST_ROOT "/usr/local/opt/boost")
        set(Boost_INCLUDE_DIR "/usr/local/opt/boost/include")
    endif()
endif()
find_package(Boost REQUIRED)

# OpenSSL 
if(WIN32 AND DEFINED ENV{MSYSTEM})
    # MSYS2 environment
    set(OPENSSL_ROOT_DIR "/ucrt64")
    set(OPENSSL_INCLUDE_DIR "/ucrt64/include")
    find_package(OpenSSL CONFIG REQUIRED)
elseif(APPLE)
    # macOS - Homebrew OpenSSL (not the system one)
    if(EXISTS "/opt/homebrew/opt/openssl@3")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
        set(OPENSSL_INCLUDE_DIR "/opt/homebrew/opt/openssl@3/include")
    elseif(EXISTS "/opt/homebrew/opt/openssl")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl")
        set(OPENSSL_INCLUDE_DIR "/opt/homebrew/opt/openssl/include")
    elseif(EXISTS "/usr/local/opt/openssl@3")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@3")
        set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl@3/include")
    elseif(EXISTS "/usr/local/opt/openssl")
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
        set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl/include")
    endif()
    find_package(OpenSSL REQUIRED)
else()
    # Linux and other Unix systems
    find_package(OpenSSL REQUIRED)
endif()

message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
set(MITMQTT_HAS_SSL ON)

find_package(OpenGL REQUIRED)

include(FetchContent)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.89.9
)
FetchContent_MakeAvailable(imgui)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# nlohmann-json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Compiler warnings
if(MITMQTT_ENABLE_WARNINGS)
    if(MSVC)
        add_compile_options(/W4)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
endif()

# Subprojects
add_subdirectory(src)

if(MITMQTT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
