#include "session.hpp"

namespace mitmqtt {

Session::Session(const std::string& clientId)
    : clientId_(clientId)
    , authenticated_(false) {
}

Session::~Session() {
}

std::string Session::getClientId() const {
    return clientId_;
}

void Session::setUsername(const std::string& username) {
    username_ = username;
}

std::string Session::getUsername() const {
    return username_;
}

bool Session::isAuthenticated() const {
    return authenticated_;
}

void Session::setAuthenticated(bool authenticated) {
    authenticated_ = authenticated;
}

}  
