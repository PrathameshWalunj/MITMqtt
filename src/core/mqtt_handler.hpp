  
#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <functional>

namespace mitmqtt {
// Forward declarations
class MQTTPacket;
class MQTTConnection;

// Callback types
using PacketCallback = std::function<void(const MQTTPacket&)>;
using ConnectionCallback = std::function<void(std::shared_ptr<MQTTConnection>)>;
// MQTT packet types
enum class PacketType : uint8_t {
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5,
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14
};
class MQTTHandler {
public:
    MQTTHandler(boost::asio::io_context& ioc);
    ~MQTTHandler();
    // Start listening for MQTT connections
    void start(const std::string& address, uint16_t port);
    
    // Stop the handler
    void stop();

    // Set callbacks
    void setPacketCallback(PacketCallback callback);
    void setConnectionCallback(ConnectionCallback callback);

    // Manual packet modification/injection
    void modifyPacket(std::shared_ptr<MQTTConnection> conn, const MQTTPacket& packet);
    void injectPacket(std::shared_ptr<MQTTConnection> conn, const MQTTPacket& packet);
