#pragma once

#include "core/Core.hpp"

struct EggType {
    int dropCrateId;
    int effectId;
    int duration;
    int regen;
};

namespace Eggs {
    extern std::unordered_map<int, EggType> EggTypes;

    void init();

    void eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration);
}
