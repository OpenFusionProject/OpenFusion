#pragma once

#include "CNShardServer.hpp"
#include "NPCManager.hpp"

#include <unordered_map>

struct WarpLocation;

struct TransportRoute {
    int type, start, end, cost, mssSpeed, mssRouteNum;
};

struct TransportLocation {
    int npcID, x, y, z;
};

namespace TransportManager {
    extern std::map<int32_t, TransportRoute> Routes;
    extern std::map<int32_t, TransportLocation> Locations;
    extern std::map<int32_t, std::queue<WarpLocation>> SkywayPaths; // predefined skyway paths with points
    extern std::unordered_map<CNSocket*, std::queue<WarpLocation>> SkywayQueues; // player sockets with queued broomstick points

    void init();

    void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data);
    void transportWarpHandler(CNSocket* sock, CNPacketData* data);

    void tickSkywaySystem(CNServer* serv, time_t currTime);
}
