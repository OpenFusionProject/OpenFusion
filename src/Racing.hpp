#pragma once

#include "core/Core.hpp"

#include <map>
#include <vector>
#include <set>

struct EPInfo {
    // available through XDT (maxScore may be updated by drops)
    int zoneX, zoneY, EPID, maxScore;
    // available through drops
    int maxTime, maxPods;
    double scaleFactor, podFactor, timeFactor;
};

struct EPRace {
    std::set<int> collectedRings;
    int mode, ticketSlot;
    time_t startTime;
};

namespace Racing {
    extern std::map<int32_t, EPInfo> EPData;
    extern std::map<CNSocket*, EPRace> EPRaces;
    extern std::map<int32_t, std::pair<std::vector<int>, std::vector<int>>> EPRewards;

    void init();
}
