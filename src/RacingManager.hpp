#pragma once

#include "CNShardServer.hpp"

struct EPInfo {
    int zoneX, zoneY, EPID, maxScore;
};

namespace RacingManager {

    extern std::map<int32_t, EPInfo> EPData;

    void init();
}
