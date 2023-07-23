#pragma once

#include <set>

#include "servers/CNShardServer.hpp"

struct EPInfo {
    // available through XDT (maxScore may be updated by drops)
    int zoneX, zoneY, EPID, maxScore;
    // (maybe) available through drops
    int maxTime = 0, maxPods = 0;
    double scaleFactor = 0.0, podFactor = 0.0, timeFactor = 0.0;
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
