#pragma once

#include "CNProtocol.hpp"

#include <list>
#include <mutex>

namespace Monitor {
    SOCKET init();
    void tick(CNServer *, time_t);
    bool acceptConnection(SOCKET, uint16_t);
};
