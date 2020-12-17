#pragma once
#include <map>

#include "contrib/JSON.hpp"
#include "NPCManager.hpp"

namespace TableData {
    extern std::map<int32_t, std::vector<WarpLocation>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, int> RunningNPCMapNumbers;
    extern std::map<int32_t, BaseNPC*> RunningMobs;
    extern std::map<int32_t, BaseNPC*> RunningGroups;
    extern std::map<int32_t, BaseNPC*> RunningEggs;

    void init();
    void cleanup();
    void loadGruntwork(int32_t*);
    void flush();

    void loadPaths(int*);
    void loadDrops();
    void loadEggs(int32_t* nextId);
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathNPC(nlohmann::json::iterator, int id=0);
}
