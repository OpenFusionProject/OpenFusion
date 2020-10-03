#pragma once
#include <map>
#include <stack>

#include "contrib/JSON.hpp"
#include "NPCManager.hpp"

namespace TableData {
    extern std::map<int32_t, std::stack<WarpLocation>> RunningSkywayRoutes;

    void init();
    void cleanup();

    int getItemType(int);
    void loadPaths(int*);
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathSlider(nlohmann::json, int, int);
    void constructPathNPC(nlohmann::json::iterator);
}
