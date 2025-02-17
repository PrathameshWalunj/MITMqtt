# Create ImGui library target
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
        ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(imgui_impl
    PUBLIC
        glfw
        ${OPENGL_LIBRARIES}
)

# Add library target
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
        ${OPENGL_INCLUDE_DIR}
        ${async_mqtt5_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MITMqtt_lib
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        ${OPENGL_LIBRARIES}
        Boost::boost
    PUBLIC
        imgui_impl
        glfw
)

# Add executable target
add_executable(MITMqtt
    main.cpp
)

target_include_directories(MITMqtt
    PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${OPENGL_INCLUDE_DIR}
        ${async_mqtt5_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(MITMqtt
    PRIVATE
        MITMqtt_lib
        ${OPENGL_LIBRARIES}
        Boost::boost
)