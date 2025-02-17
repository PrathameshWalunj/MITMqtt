
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <memory>

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
    Application() {
        // initialize the GLFW library
        initializeGLFW();
        // create window using GLFW
        createWindow();
        // initialize ImGui
        initializeImGui();
    }

    ~Application() {
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
        static bool show_demo_window = true;
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
                ImGui::MenuItem("Show Demo Window", nullptr, &show_demo_window);
                ImGui::MenuItem("Show Intercept Window", nullptr, &show_intercept_window);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // demo window 
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
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
            if (ImGui::Button(interceptEnabled ? "Stop" : "Start")) {
                interceptEnabled = !interceptEnabled;
                // TODO: figuring this out (implement actual proxy start/stop)
            }

            ImGui::End();
        }
    }

private:
    std::unique_ptr<GLFWwindow, GLFWwindowDeleter> window;
    const char* glsl_version_;
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