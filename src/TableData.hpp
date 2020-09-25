#pragma once
#include <map>

#include "contrib/JSON.hpp"

namespace TableData {
    void init();
    void cleanup();

    int getItemType(int);
    void loadPaths();
    void constructPathSkyway(nlohmann::json::iterator);
    void constructPathNPC(nlohmann::json::iterator);
}
