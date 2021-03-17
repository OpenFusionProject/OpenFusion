#pragma once

#include "core/Core.hpp"

namespace BuiltinCommands {
    void init();

    void setSpecialState(CNSocket *sock, CNPacketData *data);
};
