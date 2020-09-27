#pragma once
#include <map>

#include "contrib/JSON.hpp"

namespace TableData {
    void init();
    void cleanup();

    int getItemType(int);
    void loadPaths(int*);
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathSlider(nlohmann::json, int, int);
    void constructPathNPC(nlohmann::json::iterator);
}
