#pragma once

#include "core/Core.hpp"

namespace Monitor {
    SOCKET init();
    bool acceptConnection(SOCKET, uint16_t);
};
