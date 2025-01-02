#pragma once

#include "core/Core.hpp"

namespace Monitor {
    extern std::vector<std::string> chats;
    extern std::vector<std::string> bcasts;
    extern std::vector<std::string> emails;

    SOCKET init();
    bool acceptConnection(SOCKET, uint16_t);
};
