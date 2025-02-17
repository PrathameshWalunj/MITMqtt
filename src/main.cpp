
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "core/mqtt_handler.hpp"

#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <queue>


namespace {
    // store captured packets for display
    struct PacketInfo {
        // client -> broker or vv
        std::string direction;
        // CONNECT, PUBLISH etc
        std::string type;
        // packet content
        std::string payload;
        // timestamp
        std::string timestamp;
    };
    std::vector<PacketInfo> capturedPackets;
}

// forward declaration generally better
class Window;

// custom deleter for GLFW window when using smart pointer
struct GLFWwindowDeleter {
    void operator()(GLFWwindow* window) {
        glfwDestroyWindow(window);
    }
};

class Application {
public:
    Application()
        : ioc_()
        , mqtt_handler_(ioc_)
        , work_guard_(boost::asio::make_work_guard(ioc_)) {
        // initialize the GLFW library
        initializeGLFW();
        // create window using GLFW
        createWindow();
        // initialize ImGui
        initializeImGui();

        // start io context in diff thread
        io_thread_ = std::thread([this]() {
            ioc_.run();
        });
    }

    ~Application() {

        // stop mqtt handler
        mqtt_handler_.stop();

        // stop io context
        work_guard_.reset();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        // cleanup ImGui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        // GLFW cleanup happens automatically through unique ptr
        glfwTerminate();
    }

    void run() {
        while (!glfwWindowShouldClose(window.get())) {
            // poll and handle events
            // handle user inputs
            glfwPollEvents();

            // start the new ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // render the main window UI
            renderMainWindow();

            // rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window.get(), &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            // swap buffers to display the frame
            glfwSwapBuffers(window.get());
        }
    }

private:
  void initializeGLFW() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // decide GL+GLSL versions
        #if defined(IMGUI_IMPL_OPENGL_ES2)
            const char* glsl_version = "#version 100";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
        #elif defined(__APPLE__)
            const char* glsl_version = "#version 150";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        #else
            const char* glsl_version = "#version 130";
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        #endif
        glsl_version_ = glsl_version;
    }

    void createWindow() {
        // create main application window
        window.reset(glfwCreateWindow(1280, 720, "MITMqtt - MQTT Intercepting Proxy", nullptr, nullptr));
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window.get());
        // enable vsync
        glfwSwapInterval(1); 
    }

    void initializeImGui() {
        // initialize ImGui context and configure settings
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        // enable keyboard controls 
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   

        // choice
        ImGui::StyleColorsDark();

        // setup platform/renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window.get(), true);
        ImGui_ImplOpenGL3_Init(glsl_version_);
    }

    void renderMainWindow() {
        static bool show_packet_window = true;
        static bool show_intercept_window = true;
        
        // main menu bar (top)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window.get(), true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Show Packet Window", nullptr, &show_packet_window);
                ImGui::MenuItem("Show Intercept Window", nullptr, &show_intercept_window);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

       
       
        // intercept window
        if (show_intercept_window) {
            ImGui::Begin("MQTT Interceptor", &show_intercept_window);
            
            // connection status
            static bool interceptEnabled = false;
            ImGui::Checkbox("Enable Interception", &interceptEnabled);
            
            // basic connection info
            static char listenAddress[128] = "0.0.0.0";
            static int listenPort = 1883;
            ImGui::InputText("Listen Address", listenAddress, IM_ARRAYSIZE(listenAddress));
            ImGui::InputInt("Listen Port", &listenPort);

            // broker settings
            static char brokerAddress[128] = "test.mosquitto.org";
            static int brokerPort = 1883;
            ImGui::InputText("Broker Address", brokerAddress, IM_ARRAYSIZE(brokerAddress));
            ImGui::InputInt("Broker Port", &brokerPort);

            // start stop intercept button
            if (ImGui::Button(interceptEnabled ? "Stop Intercepting" : "Start Intercepting")) {
                if (!interceptEnabled) {
                    try {
                        mqtt_handler_.start(listenAddress, static_cast<uint16_t>(listenPort));
                        interceptEnabled = true;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Failed to start MQTT handler: " << e.what() << std::endl;

                    }
                }
                else {
                    mqtt_handler_.stop();
                    interceptEnabled = false;
                }
            }

            ImGui::End();
        }

        if (show_packet_window) {
            ImGui::Begin("MQTT Packets", &show_packet_window);

            if (ImGui::BeginTable("Packets", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Time");
                ImGui::TableSetupColumn("Direction");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Payload");
                ImGui::TableHeadersRow();

                for (const auto& packet : capturedPackets) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(packet.timestamp.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(packet.direction.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(packet.type.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(packet.payload.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }
    }

private:
    std::unique_ptr<GLFWwindow, GLFWwindowDeleter> window;
    const char* glsl_version_;

    boost::asio::io_context ioc_;
    mitmqtt::MQTTHandler mqtt_handler_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::thread io_thread_;
};

int main([[maybe_unused]]int argc,[[maybe_unused]] char** argv) {
    try {
        Application app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}