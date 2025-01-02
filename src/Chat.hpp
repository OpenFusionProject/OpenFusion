#pragma once

#define CMD_PREFIX '/'

#include "core/Core.hpp"

#include <string>
#include <vector>

namespace Chat {
    extern std::vector<std::string> dump;
    extern std::vector<std::string> bcasts;
    void init();

    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD
    std::string sanitizeText(std::string text, bool allowNewlines=false);
}
