#pragma once

#include "servers/CNShardServer.hpp"

#include <unordered_map>

const int SLIDER_SPEED = 1200;
const int SLIDER_STOP_TICKS = 16;
const int SLIDER_GAP_SIZE = 45000;

struct Vec3 {
    int x, y, z;
};

struct WarpLocation {
    int x, y, z, instanceID, isInstance, limitTaskID, npcID;
};

struct TransportRoute {
    int type, start, end, cost, mssSpeed, mssRouteNum;
};

struct TransportLocation {
    int npcID, x, y, z;
};

namespace Transport {
    extern std::map<int32_t, TransportRoute> Routes;
    extern std::map<int32_t, TransportLocation> Locations;
    extern std::map<int32_t, std::queue<Vec3>> SkywayPaths; // predefined skyway paths with points
    extern std::unordered_map<CNSocket*, std::queue<Vec3>> SkywayQueues; // player sockets with queued broomstick points
    extern std::unordered_map<int32_t, std::queue<Vec3>> NPCQueues; // NPC ids with queued pathing points

    void init();

    void testMssRoute(CNSocket *sock, std::vector<Vec3>* route);

    void lerp(std::queue<Vec3>*, Vec3, Vec3, int, float);
    void lerp(std::queue<Vec3>*, Vec3, Vec3, int);
}
