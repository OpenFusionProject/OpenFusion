#pragma once

#include "CNProtocol.hpp"

namespace BuiltinCommands {
    void init();

    void setSpecialState(CNSocket *sock, CNPacketData *data);
};
