#pragma once

#include "core/Core.hpp"
#include "Entities.hpp"

struct EggType {
    int dropCrateId;
    int effectId;
    int duration;
    int regen;
};

namespace Eggs {
    extern std::map<std::pair<CNSocket*, int32_t>, time_t> EggBuffs;
    extern std::unordered_map<int, EggType> EggTypes;

    void init();

    /// returns -1 on fail
    int eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration);
    void npcDataToEggData(int x, int y, int z, sNPCAppearanceData* npc, sShinyAppearanceData* egg);
}
