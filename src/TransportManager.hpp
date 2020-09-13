#pragma once

#include "CNShardServer.hpp"

namespace TransportManager {
    void init();

    void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data);
    void transportWarpHandler(CNSocket* sock, CNPacketData* data);
}
