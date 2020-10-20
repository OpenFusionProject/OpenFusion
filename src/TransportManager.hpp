#pragma once

#include "CNShardServer.hpp"
#include "NPCManager.hpp"

#include <unordered_map>

const int SLIDER_SPEED = 1200;
const int SLIDER_STOP_TICKS = 16;

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
    extern std::unordered_map<int32_t, std::queue<WarpLocation>> NPCQueues; // NPC ids with queued pathing points

    void init();

    void transportRegisterLocationHandler(CNSocket*, CNPacketData*);
    void transportWarpHandler(CNSocket*, CNPacketData*);

    void testMssRoute(CNSocket *sock, std::vector<WarpLocation>* route);

    void tickTransportationSystem(CNServer*, time_t);
    void stepNPCPathing();
    void stepSkywaySystem();

    void lerp(std::queue<WarpLocation>*, WarpLocation, WarpLocation, int, float);
    void lerp(std::queue<WarpLocation>*, WarpLocation, WarpLocation, int);
}
