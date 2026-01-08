# MITMqtt

A Man-in-the-Middle proxy for MQTT protocol with TLS/SSL interception capabilities. Designed for IoT security testing, protocol analysis, and debugging MQTT-based applications. Intercept, modify, and replay live MQTT traffic.

**Website:** [https://mitmqtt.com](https://mitmqtt.com)

## Downloads

Pre-built binaries are available on the [Releases](https://github.com/PrathameshWalunj/MITMqtt/releases) page:

- Windows (x64)
- Linux (x64)  
- macOS (ARM64)

## Features

- **MQTT Packet Interception** - Capture all MQTT traffic between clients and brokers
- **TLS/SSL Decryption** - Intercept encrypted MQTTS connections (port 8883)
- **Real-time Packet Display** - View CONNECT, PUBLISH, SUBSCRIBE, and all MQTT packet types
- **Packet Injection** - Send custom MQTT packets to clients or brokers
- **Packet Modification** - Edit and replay captured packets
- **Export Functionality** - Save captured packets to log files for analysis
- **Self-Signed CA Generation** - Automatically generate certificates for TLS interception

## Requirements

### Build Dependencies

- CMake 3.14+
- C++17 compatible compiler (GCC 8+, MSVC 2019+, Clang 8+)
- Boost 1.70+ (Asio component)
- OpenSSL 1.1+ or 3.0+
- GLFW3
- OpenGL

### Recommended Build Environment

**Windows (MSYS2 UCRT64)**
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-glfw
```

## Building

```bash
# Clone the repository
git clone https://github.com/PrathameshWalunj/MITMqtt.git
cd MITMqtt


mkdir build
cd build


cmake ..
cmake --build . --config Release


./src/MITMqtt.exe  # Windows
./src/MITMqtt      # Linux/macOS
```

## Usage

### Basic MQTT Interception

1. Launch MITMqtt
2. Configure the target broker (default: test.mosquitto.org:1883)
3. Click "Start Intercepting"
4. Configure your MQTT client to connect through the proxy (default: localhost:1883)

### TLS Interception (MQTTS)

1. Launch MITMqtt
2. Check "Enable TLS (MQTTS)"
3. Click "Generate CA Certificate" (creates mitmqtt_ca.crt and mitmqtt_ca.key)
4. Click "Start Intercepting"
5. Configure your MQTT client:
   - Connect to the proxy on port 8883
   - Trust the generated CA certificate OR disable certificate verification

### Ports

| Port | Protocol | Description |
|------|----------|-------------|
| 1883 | MQTT     | Plain MQTT proxy listener |
| 8883 | MQTTS    | TLS-encrypted MQTT proxy listener |

### Packet Injection

1. Capture some packets from a connected client
2. In the Packet Editor section, enter:
   - Topic (e.g., `device/command`)
   - Payload (e.g., `{"action":"restart"}`)
3. Click "Send to Client" to inject toward the device
4. Click "Send to Broker" to inject toward the broker

## Testing with Arduino/ESP8266

Example test sketches are provided in the `tests/` directory:

- `arduino_test_suite.ino` - Comprehensive plain MQTT test
- `arduino_tls_test.ino` - TLS connection test
- `iot_sensor_test.ino` - IoT sensor simulation with command handling

### ESP8266 TLS Configuration

```cpp
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

WiFiClientSecure espClient;
espClient.setInsecure();  // Accept any certificate (for testing)
// OR: espClient.setCACert(caCertificate);  // Verify proxy CA

PubSubClient mqtt(espClient);
mqtt.setServer("PROXY_IP", 8883);
```

## Architecture

```
MQTT Client                    MITMqtt Proxy                    MQTT Broker
     |                              |                                |
     |  CONNECT (TLS:8883)          |                                |
     |----------------------------->|                                |
     |                              |  CONNECT (TCP:1883)            |
     |                              |------------------------------->|
     |                              |                                |
     |                              |           CONNACK              |
     |                              |<-------------------------------|
     |           CONNACK            |                                |
     |<-----------------------------|                                |
     |                              |                                |
     |  PUBLISH "sensor/data"       |                                |
     |----------------------------->|  [INTERCEPTED & DISPLAYED]     |
     |                              |  PUBLISH "sensor/data"         |
     |                              |------------------------------->|
```

## Security Considerations

- This tool is intended for authorized security testing only
- Generated certificates and private keys are sensitive - do not accidentally expose them
- The proxy has full visibility into decrypted traffic
- Use only on networks and devices you own or have permission to test


## Troubleshooting

**Build fails with "Boost not found"**
- Ensure Boost is installed with Asio component
- Set BOOST_ROOT environment variable if installed in non-standard location

**TLS handshake fails**
- Verify the CA certificate was generated successfully
- Check that the client is configured to trust the CA or skip verification
- Ensure port 8883 is not blocked by firewall

**No packets captured**
- Verify client is connecting to the proxy, not directly to the broker
- Check that the proxy is listening (terminal shows "Proxy started on...")

## License

MIT License


## Acknowledgments

- Built with ImGui for the graphical interface
- Uses Boost.Asio for networking
- OpenSSL for TLS/SSL support
