  
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
