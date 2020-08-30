#pragma once

#include "CNShardServer.hpp"

namespace TransportManager {
    void init();

    void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data);
}
