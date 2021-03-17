#pragma once

#include "core/Core.hpp"
#include "NPC.hpp"

struct Egg : public BaseNPC {
    bool summoned;
    bool dead = false;
    time_t deadUntil;

    Egg(int x, int y, int z, uint64_t iID, int type, int32_t id, bool summon)
        : BaseNPC(x, y, z, 0, iID, type, id) {
        summoned = summon;
        npcClass = NPCClass::NPC_EGG;
    }
};

struct EggType {
    int dropCrateId;
    int effectId;
    int duration;
    int regen;
};

namespace Eggs {
    extern std::unordered_map<int, Egg*> Eggs;
    extern std::map<std::pair<CNSocket*, int32_t>, time_t> EggBuffs;
    extern std::unordered_map<int, EggType> EggTypes;

    void init();

    /// returns -1 on fail
    int eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration);
    void npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg);
}
