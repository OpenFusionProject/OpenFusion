#pragma once

#include "CNShardServer.hpp"

struct EPInfo {
    int zoneX, zoneY, EPID, maxScore, maxTime;
};

struct EPRace {
    int ringCount, mode, ticketSlot;
    time_t startTime;
};

namespace RacingManager {
    extern std::map<int32_t, EPInfo> EPData;
    extern std::map<CNSocket*, EPRace> EPRaces;
    extern std::map<int32_t, std::pair<std::vector<int>, std::vector<int>>> EPRewards;

    void init();

    void racingStart(CNSocket* sock, CNPacketData* data);
    void racingGetPod(CNSocket* sock, CNPacketData* data);
    void racingCancel(CNSocket* sock, CNPacketData* data);
    void racingEnd(CNSocket* sock, CNPacketData* data);
}
