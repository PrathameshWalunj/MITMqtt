#include "mqtt_handler.hpp"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mitmqtt {

const char *directionToString(PacketDirection direction) {
  switch (direction) {
  case PacketDirection::ClientToBroker:
    return "Client -> Broker";
  case PacketDirection::BrokerToClient:
    return "Broker -> Client";
  default:
    return "Unknown";
  }
}

// MQTT Packet implementation
MQTTPacket MQTTPacket::fromRawData(const std::vector<uint8_t> &raw) {
  MQTTPacket packet;
  if (raw.empty())
    return packet;

  packet.data = raw;
  uint8_t firstByte = raw[0];
  packet.type = static_cast<PacketType>((firstByte >> 4) & 0x0F);
  packet.dup = (firstByte & 0x08) != 0;
  packet.qos = (firstByte >> 1) & 0x03;
  packet.retain = (firstByte & 0x01) != 0;

  // Parse PUBLISH packet for topic and payload
  if (packet.type == PacketType::PUBLISH && raw.size() > 2) {
    size_t offset = 1;

    // Decode remaining length (variable length encoding per MQTT spec)
    uint32_t remainingLength = 0;
    uint32_t multiplier = 1;
    uint8_t encodedByte;
    do {
      if (offset >= raw.size())
        return packet; // Malformed packet
      encodedByte = raw[offset++];
      remainingLength += (encodedByte & 0x7F) * multiplier;
      multiplier *= 128;
      if (multiplier > 128 * 128 * 128)
        return packet; // Malformed
    } while ((encodedByte & 0x80) != 0);

    // Read topic length
    if (offset + 2 <= raw.size()) {
      uint16_t topicLen = (raw[offset] << 8) | raw[offset + 1];
      offset += 2;

      // Read topic
      if (offset + topicLen <= raw.size()) {
        packet.topic =
            std::string(reinterpret_cast<const char *>(&raw[offset]), topicLen);
        offset += topicLen;

        // Skip packet ID for QoS > 0
        if (packet.qos > 0 && offset + 2 <= raw.size()) {
          offset += 2;
        }

        // Read payload
        if (offset < raw.size()) {
          packet.payload =
              std::string(reinterpret_cast<const char *>(&raw[offset]),
                          raw.size() - offset);
        }
      }
    }
  }

  return packet;
}

std::vector<uint8_t> MQTTPacket::toRawData() const { return data; }

// MQTTHandler implementation
MQTTHandler::MQTTHandler(boost::asio::io_context &ioc)
    : ioc_(ioc), acceptor_(ioc), running_(false),
      brokerHost_("test.mosquitto.org"), brokerPort_(1883), tlsEnabled_(false),
      brokerTLSEnabled_(false),
      serverSSLContext_(boost::asio::ssl::context::tls_server),
      clientSSLContext_(boost::asio::ssl::context::tls_client) {
  // Set default SSL options
  serverSSLContext_.set_options(boost::asio::ssl::context::default_workarounds |
                                boost::asio::ssl::context::no_sslv2 |
                                boost::asio::ssl::context::no_sslv3);

  clientSSLContext_.set_options(boost::asio::ssl::context::default_workarounds |
                                boost::asio::ssl::context::no_sslv2 |
                                boost::asio::ssl::context::no_sslv3);

  // For client context, we might want to skip verification for testing
  clientSSLContext_.set_verify_mode(boost::asio::ssl::verify_none);
}

MQTTHandler::~MQTTHandler() { stop(); }

void MQTTHandler::start(const std::string &address, uint16_t port) {
  try {
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(address), port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    running_ = true;

    std::cout << "MQTT Proxy started on " << address << ":" << port
              << std::endl;
    std::cout << "Will forward to broker: " << brokerHost_ << ":" << brokerPort_
              << std::endl;

    doAccept();
  } catch (const std::exception &e) {
    std::cerr << "Failed to start MQTT handler: " << e.what() << std::endl;
    throw;
  }
}

void MQTTHandler::stop() {
  if (!running_)
    return;

  running_ = false;

  boost::system::error_code ec;
  acceptor_.close(ec);

  // Stop all active connections
  for (auto &conn : connections_) {
    if (conn)
      conn->stop();
  }
  connections_.clear();

  std::cout << "MQTT Proxy stopped" << std::endl;
}

void MQTTHandler::setPacketCallback(PacketCallback callback) {
  packetCallback_ = std::move(callback);
}

void MQTTHandler::setConnectionCallback(ConnectionCallback callback) {
  connectionCallback_ = std::move(callback);
}

void MQTTHandler::setBrokerConfig(const std::string &host, uint16_t port) {
  brokerHost_ = host;
  brokerPort_ = port;
}

void MQTTHandler::setTLSCertificate(const std::string &certFile,
                                    const std::string &keyFile) {
  certFile_ = certFile;
  keyFile_ = keyFile;

  try {
    // Load certificate and key for server context (accepting clients)
    serverSSLContext_.use_certificate_file(certFile,
                                           boost::asio::ssl::context::pem);
    serverSSLContext_.use_private_key_file(keyFile,
                                           boost::asio::ssl::context::pem);

    std::cout << "TLS certificate loaded: " << certFile << std::endl;
    std::cout << "TLS private key loaded: " << keyFile << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to load TLS certificate/key: " << e.what()
              << std::endl;
    throw;
  }
}

void MQTTHandler::doAccept() {
  acceptor_.async_accept([this](boost::system::error_code ec,
                                boost::asio::ip::tcp::socket socket) {
    if (!ec) {
      std::cout << "New client connection from " << socket.remote_endpoint()
                << std::endl;
      handleConnection(std::move(socket));
    } else {
      std::cerr << "Accept error: " << ec.message() << std::endl;
    }

    if (running_) {
      doAccept();
    }
  });
}

void MQTTHandler::handleConnection(boost::asio::ip::tcp::socket socket) {
  auto conn = std::make_shared<MQTTConnection>(std::move(socket), *this);
  connections_.push_back(conn);

  if (connectionCallback_) {
    connectionCallback_(conn);
  }

  conn->start();
}

void MQTTHandler::startTLS(const std::string &address, uint16_t port) {
  try {
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(address), port);

    tlsAcceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(ioc_);
    tlsAcceptor_->open(endpoint.protocol());
    tlsAcceptor_->set_option(boost::asio::socket_base::reuse_address(true));
    tlsAcceptor_->bind(endpoint);
    tlsAcceptor_->listen();

    tlsEnabled_ = true;

    std::cout << "MQTTS (TLS) Proxy started on " << address << ":" << port
              << std::endl;

    doAcceptTLS();
  } catch (const std::exception &e) {
    std::cerr << "Failed to start TLS listener: " << e.what() << std::endl;
    throw;
  }
}

void MQTTHandler::doAcceptTLS() {
  if (!tlsAcceptor_)
    return;

  tlsAcceptor_->async_accept([this](boost::system::error_code ec,
                                    boost::asio::ip::tcp::socket socket) {
    if (!ec) {
      std::cout << "[TLS] New client connection from "
                << socket.remote_endpoint() << std::endl;
      handleTLSConnection(std::move(socket));
    } else {
      std::cerr << "[TLS] Accept error: " << ec.message() << std::endl;
    }

    if (running_ && tlsEnabled_) {
      doAcceptTLS();
    }
  });
}

void MQTTHandler::handleTLSConnection(boost::asio::ip::tcp::socket socket) {
  auto conn = std::make_shared<MQTTTLSConnection>(
      std::move(socket), serverSSLContext_, clientSSLContext_, *this);
  tlsConnections_.push_back(conn);
  conn->start();
}

void MQTTHandler::storePacket(const MQTTPacket &packet) {
  storedPackets_.push_back(packet);
  // Limit storage to prevent memory issues
  if (storedPackets_.size() > 1000) {
    storedPackets_.erase(storedPackets_.begin());
  }
}

void MQTTHandler::modifyPacket(const std::string &packetType,
                               const std::string &payload) {
  // For backward compatibility, inject to client with default topic test!!!
  injectPacket("mitmqtt/injected", payload, true);
}

void MQTTHandler::injectPacket(const std::string &topic,
                               const std::string &payload, bool toClient) {
  // Check if we have any connections (plain or TLS)
  bool hasPlainConn = !connections_.empty();
  bool hasTLSConn = !tlsConnections_.empty();

  if (!hasPlainConn && !hasTLSConn) {
    std::cerr << "No active connections to send packet to" << std::endl;
    return;
  }

  // Create a PUBLISH packet
  std::vector<uint8_t> packet;

  // Fixed header: PUBLISH (0x30)
  packet.push_back(0x30);

  // Calculate remaining length
  uint16_t topicLen = static_cast<uint16_t>(topic.length());
  size_t remainingLength = 2 + topicLen + payload.length();

  // Encode remaining length (variable length encoding)
  do {
    uint8_t encodedByte = remainingLength % 128;
    remainingLength /= 128;
    if (remainingLength > 0) {
      encodedByte |= 0x80;
    }
    packet.push_back(encodedByte);
  } while (remainingLength > 0);

  // Topic length (MSB, LSB)
  packet.push_back(static_cast<uint8_t>(topicLen >> 8));
  packet.push_back(static_cast<uint8_t>(topicLen & 0xFF));

  // Topic
  packet.insert(packet.end(), topic.begin(), topic.end());

  // Payload
  packet.insert(packet.end(), payload.begin(), payload.end());

  // Send to client or broker - prefer TLS connection if available
  if (hasTLSConn) {
    if (toClient) {
      tlsConnections_[0]->sendToClient(packet);
      std::cout << "[TLS] Injected to CLIENT - Topic: " << topic
                << ", Payload: " << payload << std::endl;
    } else {
      tlsConnections_[0]->sendToBroker(packet);
      std::cout << "[TLS] Injected to BROKER - Topic: " << topic
                << ", Payload: " << payload << std::endl;
    }
  } else if (hasPlainConn) {
    if (toClient) {
      connections_[0]->sendToClient(packet);
      std::cout << "Injected to CLIENT - Topic: " << topic
                << ", Payload: " << payload << std::endl;
    } else {
      connections_[0]->sendToBroker(packet);
      std::cout << "Injected to BROKER - Topic: " << topic
                << ", Payload: " << payload << std::endl;
    }
  }
}

void MQTTHandler::replayPacket(int packetIndex) {
  if (packetIndex < 0 ||
      packetIndex >= static_cast<int>(storedPackets_.size())) {
    std::cerr << "Invalid packet index: " << packetIndex << std::endl;
    return;
  }

  if (connections_.empty()) {
    std::cerr << "No active connections to replay packet to" << std::endl;
    return;
  }

  const auto &packet = storedPackets_[packetIndex];
  connections_[0]->sendToClient(packet.toRawData());

  std::cout << "Replayed packet " << packetIndex << std::endl;
}

// MQTTConnection implementation
MQTTConnection::MQTTConnection(boost::asio::ip::tcp::socket socket,
                               MQTTHandler &handler)
    : clientSocket_(std::move(socket)),
      brokerSocket_(clientSocket_.get_executor()), handler_(handler),
      connected_(false), brokerConnected_(false) {
  clientReadBuffer_.resize(8192);
  brokerReadBuffer_.resize(8192);
}

void MQTTConnection::start() {
  connected_ = true;
  // Don't connect to broker yet - wait for CONNECT packet
  doReadFromClient();
}

void MQTTConnection::stop() {
  if (!connected_)
    return;

  connected_ = false;
  brokerConnected_ = false;

  boost::system::error_code ec;
  clientSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  brokerSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

  clientSocket_.close(ec);
  brokerSocket_.close(ec);

  std::cout << "Connection closed" << std::endl;
}

void MQTTConnection::connectToBroker(const std::string &host, uint16_t port) {
  if (brokerConnected_)
    return;

  try {
    boost::asio::ip::tcp::resolver resolver(brokerSocket_.get_executor());
    auto endpoints = resolver.resolve(host, std::to_string(port));

    boost::asio::connect(brokerSocket_, endpoints);
    brokerConnected_ = true;

    std::cout << "Connected to broker: " << host << ":" << port << std::endl;

    // Start reading from broker
    doReadFromBroker();
  } catch (const std::exception &e) {
    std::cerr << "Failed to connect to broker: " << e.what() << std::endl;
    stop();
  }
}

void MQTTConnection::handlePacket(const std::vector<uint8_t> &data,
                                  PacketDirection direction) {
  if (data.empty())
    return;

  MQTTPacket packet = MQTTPacket::fromRawData(data);

  // Store packet for replay
  handler_.storePacket(packet);

  uint8_t packetType = (data[0] >> 4) & 0x0F;
  std::string packetTypeStr;
  std::string payload = packet.payload;

  switch (packetType) {
  case 1:
    packetTypeStr = "CONNECT";
    break;
  case 2:
    packetTypeStr = "CONNACK";
    break;
  case 3:
    packetTypeStr = "PUBLISH";
    payload = "Topic: " + packet.topic + ", Payload: " + packet.payload;
    break;
  case 4:
    packetTypeStr = "PUBACK";
    break;
  case 8:
    packetTypeStr = "SUBSCRIBE";
    break;
  case 9:
    packetTypeStr = "SUBACK";
    break;
  case 10:
    packetTypeStr = "UNSUBSCRIBE";
    break;
  case 11:
    packetTypeStr = "UNSUBACK";
    break;
  case 12:
    packetTypeStr = "PINGREQ";
    break;
  case 13:
    packetTypeStr = "PINGRESP";
    break;
  case 14:
    packetTypeStr = "DISCONNECT";
    break;
  default:
    packetTypeStr = "OTHER (" + std::to_string(packetType) + ")";
  }

  std::cout << directionToString(direction) << " - " << packetTypeStr
            << std::endl;

  // Call callback
  if (handler_.packetCallback_) {
    handler_.packetCallback_(direction, packetTypeStr, payload);
  }
}

void MQTTConnection::doReadFromClient() {
  if (!connected_)
    return;

  auto self = shared_from_this();
  clientSocket_.async_read_some(
      boost::asio::buffer(clientReadBuffer_),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (ec) {
          std::cerr << "Client read error: " << ec.message() << std::endl;
          stop();
          return;
        }

        std::vector<uint8_t> data(clientReadBuffer_.begin(),
                                  clientReadBuffer_.begin() + length);

        // Handle packet
        handlePacket(data, PacketDirection::ClientToBroker);

        // Check if this is a CONNECT packet and we need to connect to broker
        if (!brokerConnected_ && data.size() > 0 &&
            ((data[0] >> 4) & 0x0F) == 1) {
          // This is a CONNECT packet, now connect to broker
          connectToBroker(handler_.getBrokerHost(), handler_.getBrokerPort());
        }

        // Forward to broker if connected
        if (brokerConnected_) {
          sendToBroker(data);
        }

        // Continue reading
        doReadFromClient();
      });
}

void MQTTConnection::doReadFromBroker() {
  if (!brokerConnected_)
    return;

  auto self = shared_from_this();
  brokerSocket_.async_read_some(
      boost::asio::buffer(brokerReadBuffer_),
      [this, self](boost::system::error_code ec, std::size_t length) {
        if (ec) {
          std::cerr << "Broker read error: " << ec.message() << std::endl;
          stop();
          return;
        }

        std::vector<uint8_t> data(brokerReadBuffer_.begin(),
                                  brokerReadBuffer_.begin() + length);

        // Handle packet
        handlePacket(data, PacketDirection::BrokerToClient);

        // Forward to client
        sendToClient(data);

        // Continue reading
        doReadFromBroker();
      });
}

void MQTTConnection::sendToClient(const std::vector<uint8_t> &data) {
  if (!connected_)
    return;

  auto self = shared_from_this();
  boost::asio::async_write(
      clientSocket_, boost::asio::buffer(data),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "Client write error: " << ec.message() << std::endl;
          stop();
        }
      });
}

void MQTTConnection::sendToBroker(const std::vector<uint8_t> &data) {
  if (!brokerConnected_)
    return;

  auto self = shared_from_this();
  boost::asio::async_write(
      brokerSocket_, boost::asio::buffer(data),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "Broker write error: " << ec.message() << std::endl;
          stop();
        }
      });
}

std::string MQTTConnection::getClientId() const { return clientId_; }

std::string MQTTConnection::getClientAddress() const {
  try {
    return clientSocket_.remote_endpoint().address().to_string();
  } catch (...) {
    return "unknown";
  }
}

std::string MQTTConnection::getBrokerAddress() const {
  try {
    if (brokerConnected_) {
      return brokerSocket_.remote_endpoint().address().to_string();
    }
    return "not connected";
  } catch (...) {
    return "unknown";
  }
}

// ============================================================================
// MQTTTLSConnection Implementation
// ============================================================================

MQTTTLSConnection::MQTTTLSConnection(boost::asio::ip::tcp::socket socket,
                                     boost::asio::ssl::context &serverCtx,
                                     boost::asio::ssl::context & /*clientCtx*/,
                                     MQTTHandler &handler)
    : clientStream_(std::move(socket), serverCtx),
      brokerSocket_(clientStream_.get_executor()), handler_(handler),
      clientReadBuffer_(8192), brokerReadBuffer_(8192), connected_(false),
      brokerConnected_(false) {
  std::cout << "TLS connection created (TLS termination mode)" << std::endl;
}

void MQTTTLSConnection::start() { doHandshakeWithClient(); }

void MQTTTLSConnection::stop() {
  connected_ = false;
  brokerConnected_ = false;

  boost::system::error_code ec;

  // Shutdown SSL gracefully for client
  clientStream_.shutdown(ec);

  // Close sockets
  clientStream_.lowest_layer().close(ec);
  brokerSocket_.close(ec);
}

void MQTTTLSConnection::doHandshakeWithClient() {
  auto self = shared_from_this();

  clientStream_.async_handshake(
      boost::asio::ssl::stream_base::server,
      [this, self](boost::system::error_code ec) {
        if (!ec) {
          std::cout << "TLS handshake with client successful" << std::endl;
          connected_ = true;
          doConnectToBroker();
        } else {
          std::cerr << "TLS handshake with client failed: " << ec.message()
                    << std::endl;
          stop();
        }
      });
}

void MQTTTLSConnection::doConnectToBroker() {
  auto self = shared_from_this();

  boost::asio::ip::tcp::resolver resolver(clientStream_.get_executor());
  auto endpoints = resolver.resolve(handler_.getBrokerHost(),
                                    std::to_string(handler_.getBrokerPort()));

  boost::asio::async_connect(
      brokerSocket_, endpoints,
      [this, self](boost::system::error_code ec,
                   const boost::asio::ip::tcp::endpoint &endpoint) {
        if (!ec) {
          std::cout << "Connected to broker (plain TCP): " << endpoint
                    << std::endl;
          brokerConnected_ = true;

          // Start reading from both sides immediately (no TLS handshake needed)
          doReadFromClient();
          doReadFromBroker();
        } else {
          std::cerr << "Failed to connect to broker: " << ec.message()
                    << std::endl;
          stop();
        }
      });
}

void MQTTTLSConnection::doReadFromClient() {
  if (!connected_)
    return;

  auto self = shared_from_this();
  clientStream_.async_read_some(
      boost::asio::buffer(clientReadBuffer_),
      [this, self](boost::system::error_code ec,
                   std::size_t bytes_transferred) {
        if (!ec && bytes_transferred > 0) {
          std::vector<uint8_t> data(clientReadBuffer_.begin(),
                                    clientReadBuffer_.begin() +
                                        bytes_transferred);

          // Process and log the packet
          handlePacket(data, PacketDirection::ClientToBroker);

          // Forward to broker
          sendToBroker(data);

          // Continue reading
          doReadFromClient();
        } else if (ec) {
          std::cerr << "TLS Client read error: " << ec.message() << std::endl;
          stop();
        }
      });
}

void MQTTTLSConnection::doReadFromBroker() {
  if (!brokerConnected_)
    return;

  auto self = shared_from_this();
  brokerSocket_.async_read_some(
      boost::asio::buffer(brokerReadBuffer_),
      [this, self](boost::system::error_code ec,
                   std::size_t bytes_transferred) {
        if (!ec && bytes_transferred > 0) {
          std::vector<uint8_t> data(brokerReadBuffer_.begin(),
                                    brokerReadBuffer_.begin() +
                                        bytes_transferred);

          // Process and log the packet
          handlePacket(data, PacketDirection::BrokerToClient);

          // Forward to client
          sendToClient(data);

          // Continue reading
          doReadFromBroker();
        } else if (ec) {
          std::cerr << "TLS Broker read error: " << ec.message() << std::endl;
          stop();
        }
      });
}

void MQTTTLSConnection::handlePacket(const std::vector<uint8_t> &data,
                                     PacketDirection direction) {
  MQTTPacket packet = MQTTPacket::fromRawData(data);

  // Store the packet
  handler_.storePacket(packet);

  // Get packet type string
  std::string packetType;
  switch (packet.type) {
  case PacketType::CONNECT:
    packetType = "CONNECT";
    break;
  case PacketType::CONNACK:
    packetType = "CONNACK";
    break;
  case PacketType::PUBLISH:
    packetType = "PUBLISH";
    break;
  case PacketType::PUBACK:
    packetType = "PUBACK";
    break;
  case PacketType::PUBREC:
    packetType = "PUBREC";
    break;
  case PacketType::PUBREL:
    packetType = "PUBREL";
    break;
  case PacketType::PUBCOMP:
    packetType = "PUBCOMP";
    break;
  case PacketType::SUBSCRIBE:
    packetType = "SUBSCRIBE";
    break;
  case PacketType::SUBACK:
    packetType = "SUBACK";
    break;
  case PacketType::UNSUBSCRIBE:
    packetType = "UNSUBSCRIBE";
    break;
  case PacketType::UNSUBACK:
    packetType = "UNSUBACK";
    break;
  case PacketType::PINGREQ:
    packetType = "PINGREQ";
    break;
  case PacketType::PINGRESP:
    packetType = "PINGRESP";
    break;
  case PacketType::DISCONNECT:
    packetType = "DISCONNECT";
    break;
  default:
    packetType =
        "OTHER (" + std::to_string(static_cast<int>(packet.type)) + ")";
    break;
  }

  std::cout << "[TLS] " << directionToString(direction) << " - " << packetType
            << std::endl;

  // Build payload string for display
  std::string payloadStr;
  if (packet.type == PacketType::PUBLISH) {
    payloadStr = "Topic: " + packet.topic + ", Payload: " + packet.payload;
  }

  // Call packet callback if set
  if (handler_.packetCallback_) {
    handler_.packetCallback_(direction, packetType, payloadStr);
  }
}

void MQTTTLSConnection::sendToClient(const std::vector<uint8_t> &data) {
  if (!connected_)
    return;

  auto self = shared_from_this();
  boost::asio::async_write(
      clientStream_, boost::asio::buffer(data),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "TLS Client write error: " << ec.message() << std::endl;
          stop();
        }
      });
}

void MQTTTLSConnection::sendToBroker(const std::vector<uint8_t> &data) {
  if (!brokerConnected_)
    return;

  auto self = shared_from_this();
  boost::asio::async_write(
      brokerSocket_, boost::asio::buffer(data),
      [this, self](boost::system::error_code ec, std::size_t /*length*/) {
        if (ec) {
          std::cerr << "TLS Broker write error: " << ec.message() << std::endl;
          stop();
        }
      });
}

std::string MQTTTLSConnection::getClientId() const { return clientId_; }

std::string MQTTTLSConnection::getClientAddress() const {
  try {
    return clientStream_.lowest_layer().remote_endpoint().address().to_string();
  } catch (...) {
    return "unknown";
  }
}

std::string MQTTTLSConnection::getBrokerAddress() const {
  try {
    if (brokerConnected_) {
      return brokerSocket_.remote_endpoint().address().to_string();
    }
    return "not connected";
  } catch (...) {
    return "unknown";
  }
}

} 