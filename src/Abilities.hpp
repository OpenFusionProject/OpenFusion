#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"
#include "Player.hpp"

#include <map>
#include <vector>

constexpr size_t MAX_SKILLRESULT_SIZE = sizeof(sSkillResult_BatteryDrain);

enum class SkillEffectTarget {
    POINT = 1,
    SELF = 2,
    CONE = 3,
    WEAPON = 4,
    AREA_SELF = 5,
    AREA_TARGET = 6
};

enum class SkillTargetType {
    MOBS = 1,
    SELF = 2,
    GROUP = 3
};

enum class SkillDrainType {
    ACTIVE = 1,
    PASSIVE = 2
};

struct SkillResult {
    size_t size;
    uint8_t payload[MAX_SKILLRESULT_SIZE];
    SkillResult(size_t len, void* dat) {
        size = len;
        memcpy(payload, dat, len);
    }
    SkillResult() {
        size = 0;
    }
};

struct SkillData {
    int skillType; // eST
    SkillEffectTarget effectTarget;
    int effectType; // always 1?
    SkillTargetType targetType;
    SkillDrainType drainType;
    int effectArea;

    int batteryUse[4];
    int durationTime[4];

    int valueTypes[3];
    int values[3][4];
};

namespace Abilities {
    extern std::map<int32_t, SkillData> SkillTable;

    void useNanoSkill(CNSocket*, sNano&, std::vector<ICombatant*>);
    void useNPCSkill(EntityRef, int skillID, std::vector<ICombatant*>);

    std::vector<EntityRef> matchTargets(SkillData*, int, int32_t*);
    int getCSTBFromST(int eSkillType);
}
