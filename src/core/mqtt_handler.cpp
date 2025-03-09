#include "core/mqtt_handler.hpp"
#include <iostream>
#include <boost/bind/bind.hpp>

namespace mitmqtt {

    const char* directionToString(PacketDirection direction) {
        switch (direction) {
            case PacketDirection::ClientToBroker:
                return "Client -> Broker";
            case PacketDirection::BrokerToClient:
                return "Broker -> Client";
            default:
                return "Unknown";
        }
    }
MQTTHandler::MQTTHandler(boost::asio::io_context& ioc)
    : ioc_(ioc)
    , acceptor_(ioc)
    , running_(false) {
}

MQTTHandler::~MQTTHandler() {
    stop();
}

void MQTTHandler::start(const std::string& address, uint16_t port) {
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(address), port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    running_ = true;
    doAccept();
}

void MQTTHandler::stop() {
    if (!running_) return;
    
    running_ = false;
    acceptor_.close();

    // Stop all active connections
    for (auto& conn : connections_) {
        if (conn) conn->stop();
    }
    connections_.clear();
}

void MQTTHandler::setPacketCallback(PacketCallback callback) {
    packetCallback_ = std::move(callback);
}

void MQTTHandler::setConnectionCallback(ConnectionCallback callback) {
    connectionCallback_ = std::move(callback);
}

void MQTTHandler::doAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                handleConnection(std::move(socket));
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



MQTTConnection::MQTTConnection(boost::asio::ip::tcp::socket socket, MQTTHandler& handler)
    : clientSocket_(std::move(socket))
    , brokerSocket_(socket.get_executor())
    , handler_(handler)
    , connected_(false) {
    // Initial buffer size
    readBuffer_.resize(8192); 
}

void MQTTConnection::start() {
    connected_ = true;
    doRead();
}

void MQTTConnection::stop() {
    if (!connected_) return;
    
    connected_ = false;
    
    boost::system::error_code ec;
    clientSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    brokerSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    
    clientSocket_.close(ec);
    brokerSocket_.close(ec);
}

void MQTTConnection::doRead() {
    if (!connected_) return;

    auto self = shared_from_this();
    clientSocket_.async_read_some(
        boost::asio::buffer(readBuffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (ec) {
                stop();
                return;
            }

            // TODO: Parse MQTT packet and handle it
            // For now, I'll just forward the data
            boost::asio::async_write(
                brokerSocket_,
                boost::asio::buffer(readBuffer_, length),
                [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                    if (ec) {
                        stop();
                        return;
            if (length > 0) {
                uint8_t firstByte = readBuffer_[0];
                uint8_t packetType = (firstByte >> 4) & 0x0F;
                std::string packetTypeStr;
                
                switch (packetType) {
                    case 1: packetTypeStr = "CONNECT"; break;
                    case 2: packetTypeStr = "CONNACK"; break;
                    case 3: packetTypeStr = "PUBLISH"; break;
                    case 4: packetTypeStr = "PUBACK"; break;
                    case 8: packetTypeStr = "SUBSCRIBE"; break;
                    case 9: packetTypeStr = "SUBACK"; break;
                    case 10: packetTypeStr = "UNSUBSCRIBE"; break;
                    case 11: packetTypeStr = "UNSUBACK"; break;
                    case 12: packetTypeStr = "PINGREQ"; break;
                    case 13: packetTypeStr = "PINGRESP"; break;
                    case 14: packetTypeStr = "DISCONNECT"; break;
                    default: packetTypeStr = "OTHER";
                }
                // Extract payload (simplified)
                std::string payload = "Binary data";
                if (length > 2) {
                    // Try to convert to readable string if possible
                    std::string rawPayload(reinterpret_cast<char*>(readBuffer_.data() + 2), 
                                          std::min(length - 2, static_cast<size_t>(100)));
                    
                    // Check if it's printable
                    bool isPrintable = true;
                    for (char c : rawPayload) {
                        if (!isprint(static_cast<unsigned char>(c)) && !isspace(static_cast<unsigned char>(c))) {
                            isPrintable = false;
                            break;
                        }
                    }
                    
                    if (isPrintable) {
                        payload = rawPayload;
                    }
                }

                std::cout<< "Received packet type: " << static_cast<int> (packetType) << " (" << packetTypeStr << ")" << std::endl;
                
                // Call the packet callback
                if (handler_.packetCallback_) {
                    handler_.packetCallback_(
                        PacketDirection::ClientToBroker,
                        packetTypeStr,
                        payload
                    );
                }
                if (packetType == 1) { // CONNECT
                    // Send CONNACK
                    uint8_t connack[] = {0x20, 0x02, 0x00, 0x00};
                    boost::asio::async_write(
                        clientSocket_,
                        boost::asio::buffer(connack, sizeof(connack)),
                        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                            if (ec) {
                                stop();
                                return;
                            }
                            
                            // Log the response packet
                            if (handler_.packetCallback_) {
                                handler_.packetCallback_(
                                    PacketDirection::BrokerToClient,
                                    "CONNACK",
                                    "Connection accepted"
                                );
                            }
                            
                            // Continue reading
                            doRead();
                        });
                    return;
                }
                else if (packetType == 3) { // PUBLISH
                    // Auto-acknowledge PUBLISH (for QoS 1)
                    size_t offset = 1; 
                    while (offset < length && (readBuffer_[offset] & 0x80) != 0) {
                        offset++;
                    }
                    offset++;
                    if (offset + 2 <= length) {
                        uint16_t topicLen = (readBuffer_[offset] << 8) | readBuffer_[offset + 1];
                        offset += 2;

                        offset += topicLen;
                        uint8_t qos = (firstByte >> 1) & 0x03;
                        if(qos > 0 && offset + 2 <= length) {
                            uint16_t packetId = (readBuffer_[offset] << 8) | readBuffer_[offset + 1];
                            offset += 2;
                            if (qos == 1) {
                                uint8_t puback[] = {0x40, 0x02,
                                                    static_cast<uint8_t>(packetId >> 8),
                                                    static_cast<uint8_t>(packetId & 0xFF)};
                                boost::asio::async_write(
                                    clientSocket_,
                                    boost::asio::buffer(puback, sizeof(puback)),
                                    [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                    if (ec) {
                                        stop();
                                        return;
                                    }
                                    // Log the response packet
                                    if (handler_.packetCallback_) {
                                        handler_.packetCallback_(
                                            PacketDirection::BrokerToClient,
                                            "PUBACK",
                                            "Message acknowledged"
                                        );
                                    }
                                    // Continue reading
                                    doRead();
                                });
                            
                            
                        }
                    
                }

                if (offset < length) {
                    std::string actualPayload(reinterpret_cast<char*>(readBuffer_.data() + offset),
                                                    length - offset);
                            bool isPrintable = true;
                            for (char c : actualPayload) {
                                if (!isprint(static_cast<unsigned char>(c)) && !isspace(static_cast<unsigned char>(c))) {
                                    isPrintable = false;
                                    break;
                                }
                            }
                            if (isPrintable) {
                                payload = actualPayload;
                                // Update callback with actual payload
                                if (handler_.packetCallback_) {
                                    handler_.packetCallback_(
                                        PacketDirection::ClientToBroker,
                                        packetTypeStr,
                                        payload
                                    );
                                }
                            }
                        }
                    }

                }
                else if (packetType == 8) { // SUBSCRIBE
                    // Auto-acknowledge SUBSCRIBE
                    if (length >= 4) {
                        uint16_t packetId = (readBuffer_[2] << 8) | readBuffer_[3];
                        // Send SUBACK (simplified, assumes all subscriptions succeeded with QoS 0)
                        uint8_t suback[] = {0x90, 0x03, 
                                          static_cast<uint8_t>(packetId >> 8), 
                                          static_cast<uint8_t>(packetId & 0xFF),
                                          0x00}; // Success with QoS 0
                        boost::asio::async_write(
                            clientSocket_,
                            boost::asio::buffer(suback, sizeof(suback)),
                            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                                if (ec) {
                                    stop();
                                    return;
                                }
                                
                                // Log the response packet
                                if (handler_.packetCallback_) {
                                    handler_.packetCallback_(
                                        PacketDirection::BrokerToClient,
                                        "SUBACK",
                                        "Subscription acknowledged"
                                    );
                                }
                                
                                // Continue reading
                                doRead();
                            });
                        return;
                    }
                }
                else if (packetType == 12) { // PINGREQ
                    // Send PINGRESP
                    uint8_t pingresp[] = {0xD0, 0x00};
                    boost::asio::async_write(
                        clientSocket_,
                        boost::asio::buffer(pingresp, sizeof(pingresp)),
                        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                            if (ec) {
                                stop();
                                return;
                            }
                            // Log the response packet
                            if (handler_.packetCallback_) {
                                handler_.packetCallback_(
                                    PacketDirection::BrokerToClient,
                                    "PINGRESP",
                                    "Ping response"
                                );
                            }
                            
                            // Continue reading
                            doRead();
                        });
                    return;
                }
            }
            
            // Continue reading
            doRead();
        });
}

std::string MQTTConnection::getClientId() const {
    return clientId_;
}

std::string MQTTConnection::getClientAddress() const {
    try {
        return clientSocket_.remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

std::string MQTTConnection::getBrokerAddress() const {
    try {
        return brokerSocket_.remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

}  
