#pragma once
#include <map>

#include "contrib/JSON.hpp"
#include "NPCManager.hpp"

namespace TableData {
    extern std::map<int32_t, std::vector<WarpLocation>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;
    extern std::map<int32_t, BaseNPC*> RunningMobs;

    void init();
    void cleanup();
    void loadGruntwork(int32_t*);
    void flush();

    int getItemType(int);
    void loadPaths(int*);
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathSlider(nlohmann::json, int, int);
    void constructPathNPC(nlohmann::json::iterator);
}
