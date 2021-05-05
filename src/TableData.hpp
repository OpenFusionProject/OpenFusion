#pragma once
#include <map>

#include "NPCManager.hpp"

// these are added to the NPC's static key to avoid collisions
const int NPC_ID_OFFSET = 1;
const int MOB_ID_OFFSET = 10000;
const int MOB_GROUP_ID_OFFSET = 20000;

// typedef for JSON object because I don't want to type nlohmann::json every time
typedef nlohmann::json json;

namespace TableData {
    extern std::map<int32_t, std::vector<Vec3>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, int> RunningNPCMapNumbers;
    extern std::map<int32_t, BaseNPC*> RunningMobs;
    extern std::map<int32_t, BaseNPC*> RunningGroups;
    extern std::map<int32_t, BaseNPC*> RunningEggs;

    void init();
    void flush();
}
