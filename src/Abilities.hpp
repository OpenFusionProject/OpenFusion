#pragma once

#include "core/Core.hpp"
#include "Combat.hpp"

struct SkillData {
    int skillType;
    int targetType;
    int drainType;
    int effectArea;
    int batteryUse[4];
    int durationTime[4];
    int powerIntensity[4];
};

namespace Abilities {
    extern std::map<int32_t, SkillData> SkillTable;
}
