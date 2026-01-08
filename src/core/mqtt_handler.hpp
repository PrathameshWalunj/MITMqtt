#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace mitmqtt {

// Forward declarations
class MQTTPacket;
class MQTTConnection;
class MQTTTLSConnection;

enum class PacketDirection { ClientToBroker, BrokerToClient };

const char *directionToString(PacketDirection direction);

// Callback types
using PacketCallback = std::function<void(PacketDirection, const std::string &,
                                          const std::string &)>;
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

// Simple MQTT Packet structure
class MQTTPacket {
public:
  PacketType type;
  std::vector<uint8_t> data;
  std::string topic;
  std::string payload;
  uint8_t qos;
  bool retain;
  bool dup;

  MQTTPacket() : type(PacketType::CONNECT), qos(0), retain(false), dup(false) {}

  // Construct from raw data
  static MQTTPacket fromRawData(const std::vector<uint8_t> &raw);

  // Convert to raw data
  std::vector<uint8_t> toRawData() const;
};

class MQTTHandler {
public:
  MQTTHandler(boost::asio::io_context &ioc);
  ~MQTTHandler();

  // Start listening for MQTT connections
  void start(const std::string &address, uint16_t port);

  // Start TLS listener on specified port (usually 8883)
  void startTLS(const std::string &address, uint16_t port);

  // Stop the handler
  void stop();

  // Set callbacks
  void setPacketCallback(PacketCallback callback);
  void setConnectionCallback(ConnectionCallback callback);

  // Set broker configuration
  void setBrokerConfig(const std::string &host, uint16_t port);

  // Public for callback access
  PacketCallback packetCallback_;

  // Manual packet modification/injection
  void modifyPacket(const std::string &packetType, const std::string &payload);
  void injectPacket(const std::string &topic, const std::string &payload,
                    bool toClient);
  void replayPacket(int packetIndex);

  // Store packets for replay
  void storePacket(const MQTTPacket &packet);

  // Broker config accessors
  const std::string &getBrokerHost() const { return brokerHost_; }
  uint16_t getBrokerPort() const { return brokerPort_; }

  // TLS configuration
  void setTLSEnabled(bool enabled) { tlsEnabled_ = enabled; }
  bool isTLSEnabled() const { return tlsEnabled_; }
  void setTLSCertificate(const std::string &certFile,
                         const std::string &keyFile);
  void setBrokerTLSEnabled(bool enabled) { brokerTLSEnabled_ = enabled; }

  // Get SSL contexts
  boost::asio::ssl::context &getServerSSLContext() { return serverSSLContext_; }
  boost::asio::ssl::context &getClientSSLContext() { return clientSSLContext_; }

private:
  // Internal methods
  void doAccept();
  void doAcceptTLS();
  void handleConnection(boost::asio::ip::tcp::socket socket);
  void handleTLSConnection(boost::asio::ip::tcp::socket socket);

  // Member variables
  boost::asio::io_context &ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::unique_ptr<boost::asio::ip::tcp::acceptor>
      tlsAcceptor_; // For TLS connections
  bool running_;

  ConnectionCallback connectionCallback_;

  std::vector<std::shared_ptr<MQTTConnection>> connections_;
  std::vector<std::shared_ptr<MQTTTLSConnection>>
      tlsConnections_; // TLS connections
  std::vector<MQTTPacket> storedPackets_;

  // Broker configuration
  std::string brokerHost_;
  uint16_t brokerPort_;

  // TLS configuration
  bool tlsEnabled_;
  bool brokerTLSEnabled_;
  std::string certFile_;
  std::string keyFile_;

  // SSL contexts
  boost::asio::ssl::context
      serverSSLContext_; // For accepting client connections
  boost::asio::ssl::context clientSSLContext_; // For connecting to broker
};

// Class to manage individual MQTT connections
class MQTTConnection : public std::enable_shared_from_this<MQTTConnection> {
public:
  MQTTConnection(boost::asio::ip::tcp::socket socket, MQTTHandler &handler);

  void start();
  void stop();

  // Send a packet to either client or broker
  void sendToClient(const std::vector<uint8_t> &data);
  void sendToBroker(const std::vector<uint8_t> &data);

  // Get connection info
  std::string getClientId() const;
  std::string getClientAddress() const;
  std::string getBrokerAddress() const;

private:
  void doReadFromClient();
  void doReadFromBroker();
  void connectToBroker(const std::string &host, uint16_t port);
  void handlePacket(const std::vector<uint8_t> &data,
                    PacketDirection direction);

  boost::asio::ip::tcp::socket clientSocket_;
  boost::asio::ip::tcp::socket brokerSocket_;
  MQTTHandler &handler_;

  std::vector<uint8_t> clientReadBuffer_;
  std::vector<uint8_t> brokerReadBuffer_;

  std::string clientId_;
  bool connected_;
  bool brokerConnected_;
};

// Type alias for SSL stream
using SSLStream = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

// Class to manage individual MQTT connections over TLS
class MQTTTLSConnection
    : public std::enable_shared_from_this<MQTTTLSConnection> {
public:
  MQTTTLSConnection(boost::asio::ip::tcp::socket socket,
                    boost::asio::ssl::context &serverCtx,
                    boost::asio::ssl::context &clientCtx, MQTTHandler &handler);

  void start();
  void stop();

  // Send a packet to either client or broker
  void sendToClient(const std::vector<uint8_t> &data);
  void sendToBroker(const std::vector<uint8_t> &data);

  // Get connection info
  std::string getClientId() const;
  std::string getClientAddress() const;
  std::string getBrokerAddress() const;

private:
  void doHandshakeWithClient();
  void doConnectToBroker();
  void doReadFromClient();
  void doReadFromBroker();
  void handlePacket(const std::vector<uint8_t> &data,
                    PacketDirection direction);

  SSLStream clientStream_; // TLS connection to client
  boost::asio::ip::tcp::socket
      brokerSocket_; // Plain TCP to broker (TLS termination)
  MQTTHandler &handler_;

  std::vector<uint8_t> clientReadBuffer_;
  std::vector<uint8_t> brokerReadBuffer_;

  std::string clientId_;
  bool connected_;
  bool brokerConnected_;
};

} 