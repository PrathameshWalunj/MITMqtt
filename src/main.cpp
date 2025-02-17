  
private:
    void initializeGLFW() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    void createWindow() {
        // create main application window
        window.reset(glfwCreateWindow(1280, 720, "MITMqtt - MQTT Intercepting Proxy", nullptr, nullptr));
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
        }
