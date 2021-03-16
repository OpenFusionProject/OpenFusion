#pragma once

#include "CNProtocol.hpp"

#include <list>
#include <mutex>

namespace Monitor {
    SOCKET init();
    bool acceptConnection(SOCKET, uint16_t);
};
