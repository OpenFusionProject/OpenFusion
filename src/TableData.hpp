#pragma once
#include <map>

#include "contrib/JSON.hpp"

namespace TableData {
    void init();
    void cleanup();

    int getItemType(int);
    void constructPath(nlohmann::json::iterator);
}
