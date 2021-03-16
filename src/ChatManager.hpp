#pragma once

#define CMD_PREFIX '/'

#include "CNShardServer.hpp"

namespace ChatManager {
    extern std::vector<std::string> dump;
    void init();

    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD
    std::string sanitizeText(std::string text, bool allowNewlines=false);
}
