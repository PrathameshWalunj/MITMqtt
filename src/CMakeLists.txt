

# ImGui implementation target (glfw + opengl3 backend) 
add_library(imgui_impl
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_impl
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_impl
    PUBLIC
        glfw
        OpenGL::GL
)

# Core library 
add_library(MITMqtt_lib
    core/session.cpp
    core/mqtt_handler.cpp
    gui/window.cpp
    utils/certificate_manager.cpp
)

target_include_directories(MITMqtt_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)



target_link_libraries(MITMqtt_lib
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
    PUBLIC
        imgui_impl
        glfw
        OpenGL::GL
)

# Platform-specific libraries
if(WIN32)
    # Windows socket libraries
    target_link_libraries(MITMqtt_lib PUBLIC ws2_32 mswsock)
elseif(UNIX AND NOT APPLE)
    
    find_package(Threads REQUIRED)
    target_link_libraries(MITMqtt_lib PUBLIC Threads::Threads)
elseif(APPLE)
    
endif()

target_compile_definitions(MITMqtt_lib PRIVATE MITMQTT_HAS_SSL=1)

# --- Executable ---
add_executable(MITMqtt
    main.cpp
)

target_link_libraries(MITMqtt
    PRIVATE
        MITMqtt_lib
)
