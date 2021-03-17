#pragma once

#include <set>
#include <vector>

#include "Player.hpp"
#include "servers/CNShardServer.hpp"

struct NanoData {
    int style;
};

struct NanoTuning {
    int reqItemCount;
    int reqItems;
};

namespace Nanos {
    extern std::map<int32_t, NanoData> NanoTable;
    extern std::map<int32_t, NanoTuning> NanoTunings;
    void init();

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm=false);
    void summonNano(CNSocket* sock, int slot, bool silent = false);
    int nanoStyle(int nanoID);
    bool getNanoBoost(Player* plr);
}
