  
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
