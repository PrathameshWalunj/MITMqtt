  
#include "core/mqtt_handler.hpp"
#include <iostream>
#include <boost/bind/bind.hpp>

namespace mitmqtt {

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
                    }
                    
                    doRead();
                });
        });
}
