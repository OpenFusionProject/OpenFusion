#pragma once

#include "JSON.hpp"

#include "Entities.hpp"
#include "Transport.hpp"

#include <map>
#include <unordered_map>
#include <vector>

// these are added to the NPC's static key to avoid collisions
const int NPC_ID_OFFSET = 1;
const int MOB_ID_OFFSET = 10000;
const int MOB_GROUP_ID_OFFSET = 30000;

// typedef for JSON object because I don't want to type nlohmann::json every time
typedef nlohmann::json json;

namespace TableData {
    extern std::map<int32_t, std::vector<Vec3>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, int> RunningNPCMapNumbers;
    extern std::unordered_map<int32_t, std::pair<BaseNPC*, std::vector<BaseNPC*>>> RunningNPCPaths; // player ID -> following NPC
    extern std::vector<NPCPath> FinishedNPCPaths; // NPC ID -> path
    extern std::map<int32_t, BaseNPC*> RunningMobs;
    extern std::map<int32_t, BaseNPC*> RunningGroups;
    extern std::map<int32_t, BaseNPC*> RunningEggs;

    void init();
    void flush();
}
