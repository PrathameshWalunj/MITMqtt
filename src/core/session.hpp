  
#pragma once

#include <string>
#include <cstdint>

namespace mitmqtt {


class Session {
public:
    Session(const std::string& clientId);
    ~Session();

    std::string getClientId() const;
    void setUsername(const std::string& username);
    std::string getUsername() const;
    
    bool isAuthenticated() const;
    void setAuthenticated(bool authenticated);

private:
    std::string clientId_;
    std::string username_;
    bool authenticated_;
};

} 