#pragma once

#include "CNShardServer.hpp"
#include "NPCManager.hpp"

#include <unordered_map>

#define LERP_GAP 3000

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
    extern std::map<int32_t, std::vector<WarpLocation>> SkywayPaths;
    extern std::unordered_map<CNSocket*, std::queue<WarpLocation>> SkywayQueue;

    void init();

    void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data);
    void transportWarpHandler(CNSocket* sock, CNPacketData* data);

    void tickSkywaySystem(CNServer* serv, time_t currTime);
}
