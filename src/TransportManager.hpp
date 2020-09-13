#pragma once

#include "CNShardServer.hpp"

struct TransportRoute {
    int type, start, end, cost, mssSpeed, mssRouteNum;
};

struct TransportLocation {
    int npcID, x, y, z;
};

namespace TransportManager {
    extern std::map<int32_t, TransportRoute> Routes;
    extern std::map<int32_t, TransportLocation> Locations;

    void init();

    void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data);
    void transportWarpHandler(CNSocket* sock, CNPacketData* data);
}
