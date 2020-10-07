#pragma once
#include <map>

#include "contrib/JSON.hpp"
#include "NPCManager.hpp"

namespace TableData {
    extern std::map<int32_t, std::vector<WarpLocation>> RunningSkywayRoutes;
    extern std::map<int32_t, int> RunningNPCRotations;

    void init();
    void cleanup();
    void loadGruntwork();
    void flush();

    int getItemType(int);
    void loadPaths(int*);
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathSlider(nlohmann::json, int, int);
    void constructPathNPC(nlohmann::json::iterator);
}
