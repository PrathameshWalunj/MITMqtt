#include "core/mqtt_handler.hpp"
#include "utils/certificate_manager.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace mitmqtt {
// Store captured packets for display
struct PacketInfo {
  PacketDirection direction;
  std::string type;
  std::string payload;
  std::string timestamp;
};

std::vector<PacketInfo> capturedPackets;
std::mutex packetsMutex; 
} 

// Custom deleter for GLFW window
struct GLFWwindowDeleter {
  void operator()(GLFWwindow *window) {
    if (window) {
      glfwDestroyWindow(window);
    }
  }
};

class Application {
public:
  Application()
      : ioc_(), mqtt_handler_(ioc_),
        work_guard_(boost::asio::make_work_guard(ioc_)),
        interceptEnabled_(false) {

    // Initialize GLFW
    initializeGLFW();

    // Create window
    createWindow();

    // Initialize ImGui
    initializeImGui();

    // Set packet callback
    mqtt_handler_.setPacketCallback([this](mitmqtt::PacketDirection direction,
                                           const std::string &type,
                                           const std::string &payload) {
      // Get current timestamp
      auto now = std::chrono::system_clock::now();
      auto time = std::chrono::system_clock::to_time_t(now);
      std::string timestamp = std::ctime(&time);
      if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
      }

      // Thread-safe packet addition
      {
        std::lock_guard<std::mutex> lock(mitmqtt::packetsMutex);
        mitmqtt::PacketInfo info{direction, type, payload, timestamp};
        mitmqtt::capturedPackets.push_back(info);

        // Limit packet history
        if (mitmqtt::capturedPackets.size() > 1000) {
          mitmqtt::capturedPackets.erase(mitmqtt::capturedPackets.begin());
        }
      }
    });

    // Start IO context in separate thread
    io_thread_ = std::thread([this]() {
      try {
        ioc_.run();
      } catch (const std::exception &e) {
        std::cerr << "IO context error: " << e.what() << std::endl;
      }
    });
  }

  ~Application() {
    // Stop MQTT handler
    mqtt_handler_.stop();

    // Stop IO context
    work_guard_.reset();
    if (!ioc_.stopped()) {
      ioc_.stop();
    }

    if (io_thread_.joinable()) {
      io_thread_.join();
    }

    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // GLFW cleanup
    glfwTerminate();
  }

  void run() {
    while (!glfwWindowShouldClose(window_.get())) {
      // Poll events
      glfwPollEvents();

      // Start ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      // Render UI
      renderMainWindow();

      // Rendering
      ImGui::Render();
      int display_w, display_h;
      glfwGetFramebufferSize(window_.get(), &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(window_.get());
    }
  }

private:
  void initializeGLFW() {
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW");
    }

// Set GL version
#if defined(IMGUI_IMPL_OPENGL_ES2)
    glsl_version_ = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    glsl_version_ = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glsl_version_ = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
  }

  void createWindow() {
    window_.reset(glfwCreateWindow(
        1280, 720, "MITMqtt - MQTT Intercepting Proxy", nullptr, nullptr));
    if (!window_) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_.get());
    glfwSwapInterval(1); 
  }

  void initializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window_.get(), true);
    ImGui_ImplOpenGL3_Init(glsl_version_);
  }

  void renderMainWindow() {
    static bool show_packet_window = true;
    static bool show_intercept_window = true;
    static bool show_packet_editor = false;
    static int selected_packet = -1;
    static char modified_payload[4096] = "";
    static bool show_about = false;
    static bool show_export_success = false;
    static int exported_count = 0;
    static std::string export_path = "";
    static float export_popup_timer = 0.0f;

    // Menu bar
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Export to Log")) {
          std::lock_guard<std::mutex> lock(mitmqtt::packetsMutex);
          std::ofstream logFile("mitmqtt_capture.log");
          if (logFile.is_open()) {
            logFile << "=" << std::string(70, '=') << "\n";
            logFile << "MITMqtt Capture Log - Exported at " << __DATE__ << " "
                    << __TIME__ << "\n";
            logFile << "Total packets: " << mitmqtt::capturedPackets.size()
                    << "\n";
            logFile << "=" << std::string(70, '=') << "\n\n";

            for (size_t i = 0; i < mitmqtt::capturedPackets.size(); i++) {
              const auto &pkt = mitmqtt::capturedPackets[i];
              logFile << "[" << i << "] " << pkt.timestamp << "\n";
              logFile << "    Direction: "
                      << mitmqtt::directionToString(pkt.direction) << "\n";
              logFile << "    Type: " << pkt.type << "\n";
              logFile << "    Payload: " << pkt.payload << "\n";
              logFile << "\n";
            }
            logFile.close();
            exported_count = static_cast<int>(mitmqtt::capturedPackets.size());
            export_path = "mitmqtt_capture.log";
            show_export_success = true;
            export_popup_timer = 3.0f; // Show for 3 seconds
            std::cout << "Exported " << mitmqtt::capturedPackets.size()
                      << " packets to mitmqtt_capture.log" << std::endl;
          }
        }
        if (ImGui::MenuItem("Clear Packets")) {
          std::lock_guard<std::mutex> lock(mitmqtt::packetsMutex);
          mitmqtt::capturedPackets.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          glfwSetWindowShouldClose(window_.get(), true);
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Packet Window", nullptr, &show_packet_window);
        ImGui::MenuItem("Intercept Window", nullptr, &show_intercept_window);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
          show_about = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

   
    if (show_about) {
      ImGui::OpenPopup("About MITMqtt");
      show_about = false;
    }
    if (ImGui::BeginPopupModal("About MITMqtt", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "MITMqtt");
      ImGui::SameLine();
      ImGui::Text("- MQTT Intercepting Proxy");
      ImGui::Separator();
      ImGui::Text("Version: 0.1.0");
      ImGui::Text("Build Date: %s", __DATE__);
      ImGui::Spacing();
      ImGui::TextWrapped(
          "A transparent man-in-the-middle proxy for intercepting,");
      ImGui::TextWrapped("analyzing, and modifying MQTT traffic in real-time.");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Features:");
      ImGui::BulletText(
          "Capture MQTT packets (CONNECT, PUBLISH, SUBSCRIBE, etc.)");
      ImGui::BulletText("View packet details and payloads");
      ImGui::BulletText("Modify and replay captured packets");
      ImGui::BulletText("Export captures to log file");
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                         "Like Burp Suite, but for MQTT!");
      ImGui::Spacing();
      if (ImGui::Button("Close", ImVec2(200, 30))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

   
    if (show_export_success && export_popup_timer > 0) {
      export_popup_timer -= ImGui::GetIO().DeltaTime;
      ImGui::SetNextWindowPos(
          ImVec2(ImGui::GetIO().DisplaySize.x / 2 - 150, 50));
      ImGui::SetNextWindowSize(ImVec2(300, 0));
      ImGui::Begin("##ExportNotification", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Export Successful!");
      ImGui::Text("Exported %d packets to:", exported_count);
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s",
                         export_path.c_str());
      ImGui::End();
      if (export_popup_timer <= 0) {
        show_export_success = false;
      }
    }

    // Intercept control window
    if (show_intercept_window) {
      ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
      ImGui::Begin("MQTT Interceptor", &show_intercept_window);

      // Status indicator
      ImGui::TextColored(interceptEnabled_ ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                                           : ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                         "Status: %s",
                         interceptEnabled_ ? "● Running" : "○ Stopped");

      ImGui::Separator();
      ImGui::Text("Proxy Settings");

      // Disable inputs when running
      if (interceptEnabled_) {
        ImGui::BeginDisabled();
      }

      ImGui::InputText("Listen Address", listenAddress_,
                       IM_ARRAYSIZE(listenAddress_));
      ImGui::InputInt("Listen Port", &listenPort_);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Text("Broker Settings");

      ImGui::InputText("Broker Address", brokerAddress_,
                       IM_ARRAYSIZE(brokerAddress_));
      ImGui::InputInt("Broker Port", &brokerPort_);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TLS Settings");

      static bool tlsEnabled = false;
      static int tlsListenPort = 8883;
      static char certPath[256] = "mitmqtt_ca.crt";
      static char keyPath[256] = "mitmqtt_ca.key";
      static bool certGenerated = false;
      static std::string certStatus = "No certificate generated";

      ImGui::Checkbox("Enable TLS (MQTTS)", &tlsEnabled);

      if (tlsEnabled) {
        ImGui::InputInt("TLS Listen Port", &tlsListenPort);
        ImGui::InputText("Certificate", certPath, sizeof(certPath));
        ImGui::InputText("Private Key", keyPath, sizeof(keyPath));
      }

      // Certificate generation button
      if (ImGui::Button("Generate CA Certificate", ImVec2(-1, 30))) {
        static mitmqtt::utils::CertificateManager certManager;
        if (certManager.generateSelfSignedCertificate(certPath, keyPath)) {
          certGenerated = true;
          certStatus = "Certificate generated successfully!";
        } else {
          certGenerated = false;
          certStatus = "Failed to generate certificate";
        }
      }

      // Show certificate status
      if (certGenerated) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s",
                           certStatus.c_str());
      } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s",
                           certStatus.c_str());
      }

      if (interceptEnabled_) {
        ImGui::EndDisabled();
      }

      ImGui::Spacing();
      ImGui::Separator();

      // Start/Stop button
      if (ImGui::Button(interceptEnabled_ ? "Stop Intercepting"
                                          : "Start Intercepting",
                        ImVec2(-1, 40))) {
        if (!interceptEnabled_) {
          try {
            // Configure broker
            mqtt_handler_.setBrokerConfig(brokerAddress_,
                                          static_cast<uint16_t>(brokerPort_));

            // Start plain MQTT handler
            mqtt_handler_.start(listenAddress_,
                                static_cast<uint16_t>(listenPort_));

            // Start TLS listener if enabled
            if (tlsEnabled) {
              try {
                // Load certificate first
                mqtt_handler_.setTLSCertificate(certPath, keyPath);
                // Start TLS listener
                mqtt_handler_.startTLS(listenAddress_,
                                       static_cast<uint16_t>(tlsListenPort));
                std::cout << "TLS interception enabled on port "
                          << tlsListenPort << std::endl;
              } catch (const std::exception &e) {
                std::cerr << "TLS setup failed: " << e.what() << std::endl;
                // Continue without TLS
              }
            }

            interceptEnabled_ = true;

            std::cout << "Interception started successfully" << std::endl;
          } catch (const std::exception &e) {
            std::cerr << "Failed to start interceptor: " << e.what()
                      << std::endl;
            errorMessage_ = std::string("Failed to start: ") + e.what();
            ImGui::OpenPopup("Error");
          }
        } else {
          mqtt_handler_.stop();
          interceptEnabled_ = false;
          std::cout << "Interception stopped" << std::endl;
        }
      }

      // Error popup
      if (ImGui::BeginPopupModal("Error", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", errorMessage_.c_str());
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImGui::End();
    }

    // Packet display window
    if (show_packet_window) {
      ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
      ImGui::Begin("MQTT Packets", &show_packet_window);

      // Packet table
      if (ImGui::BeginTable("Packets", 4,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {

        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed,
                                150.0f);
        ImGui::TableSetupColumn("Direction", ImGuiTableColumnFlags_WidthFixed,
                                120.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                100.0f);
        ImGui::TableSetupColumn("Payload", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Thread-safe packet access
        std::lock_guard<std::mutex> lock(mitmqtt::packetsMutex);

        for (int i = 0; i < static_cast<int>(mitmqtt::capturedPackets.size());
             i++) {
          const auto &packet = mitmqtt::capturedPackets[i];
          bool is_selected = (i == selected_packet);

          ImGui::TableNextRow();
          ImGui::TableNextColumn();

          if (ImGui::Selectable(packet.timestamp.c_str(), is_selected,
                                ImGuiSelectableFlags_SpanAllColumns |
                                    ImGuiSelectableFlags_AllowItemOverlap)) {
            selected_packet = i;
            show_packet_editor = true;
            strncpy(modified_payload, packet.payload.c_str(),
                    sizeof(modified_payload) - 1);
            modified_payload[sizeof(modified_payload) - 1] = '\0';
          }

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(mitmqtt::directionToString(packet.direction));

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(packet.type.c_str());

          ImGui::TableNextColumn();
          // Truncate long payloads for display
          std::string displayPayload = packet.payload;
          if (displayPayload.length() > 100) {
            displayPayload = displayPayload.substr(0, 97) + "...";
          }
          ImGui::TextUnformatted(displayPayload.c_str());
        }

        ImGui::EndTable();
      }

      ImGui::Text("Total packets: %d",
                  static_cast<int>(mitmqtt::capturedPackets.size()));

      ImGui::End();
    }

    // Packet editor window
    if (show_packet_editor && selected_packet >= 0) {
      std::lock_guard<std::mutex> lock(mitmqtt::packetsMutex);

      if (selected_packet < static_cast<int>(mitmqtt::capturedPackets.size())) {
        const auto &packet = mitmqtt::capturedPackets[selected_packet];
        std::string title = "Packet Editor - " + packet.type;

        // Static variables for injection
        static char inject_topic[256] = "mitmqtt/injected";
        static bool first_load = true;

        ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin(title.c_str(), &show_packet_editor);

        // Packet info section
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                           "Original Packet Info");
        ImGui::Text("Time: %s", packet.timestamp.c_str());
        ImGui::Text("Direction: %s",
                    mitmqtt::directionToString(packet.direction));
        ImGui::Text("Type: %s", packet.type.c_str());
        ImGui::Separator();

        // Payload section
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                           "Payload (editable)");
        ImGui::InputTextMultiline("##payload", modified_payload,
                                  sizeof(modified_payload), ImVec2(-1, 150));

        ImGui::Spacing();
        ImGui::Separator();

        // Replay section
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Replay Original");
        if (ImGui::Button("Replay Original Packet", ImVec2(-1, 30))) {
          mqtt_handler_.replayPacket(selected_packet);
        }
        ImGui::TextWrapped("Sends the exact original packet again.");

        ImGui::Spacing();
        ImGui::Separator();

        // Injection section
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f),
                           "Inject Modified Packet");

        ImGui::Text("Topic:");
        ImGui::InputText("##topic", inject_topic, sizeof(inject_topic));

        ImGui::Spacing();

        // Send to Client button (inject as if coming from broker)
        if (ImGui::Button("Send to Client (as Broker)", ImVec2(-1, 35))) {
          mqtt_handler_.injectPacket(inject_topic, modified_payload, true);
        }
        ImGui::TextWrapped(
            "Injects packet to the client as if from the broker.");

        ImGui::Spacing();

        // Send to Broker button (inject as if coming from client)
        if (ImGui::Button("Send to Broker (as Client)", ImVec2(-1, 35))) {
          mqtt_handler_.injectPacket(inject_topic, modified_payload, false);
        }
        ImGui::TextWrapped(
            "Injects packet to the broker as if from the client.");

        ImGui::End();
      } else {
        show_packet_editor = false;
        selected_packet = -1;
      }
    }
  }

private:
  std::unique_ptr<GLFWwindow, GLFWwindowDeleter> window_;
  const char *glsl_version_;

  boost::asio::io_context ioc_;
  mitmqtt::MQTTHandler mqtt_handler_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
      work_guard_;
  std::thread io_thread_;

  bool interceptEnabled_;
  char listenAddress_[128] = "0.0.0.0";
  int listenPort_ = 1883;
  char brokerAddress_[128] = "test.mosquitto.org";
  int brokerPort_ = 1883;
  std::string errorMessage_;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
  try {
    std::cout << "MITMqtt - MQTT Intercepting Proxy" << std::endl;
    std::cout << "Starting application..." << std::endl;

    Application app;
    app.run();

    std::cout << "Application closed successfully" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}