#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"
#include "Player.hpp"

#include <map>
#include <vector>
#include <assert.h>

constexpr size_t MAX_SKILLRESULT_SIZE = sizeof(sSkillResult_BatteryDrain);

enum class SkillType {
    DAMAGE = 1,
    HEAL_HP = 2,
    KNOCKDOWN = 3, // dnd
    SLEEP = 4, // dnd
    SNARE = 5, // dnd
    HEAL_STAMINA = 6,
    STAMINA_SELF = 7,
    STUN = 8, // dnd
    WEAPONSLOW = 9,
    JUMP = 10,
    RUN = 11,
    STEALTH = 12,
    SWIM = 13,
    MINIMAPENEMY = 14,
    MINIMAPTRESURE = 15,
    PHOENIX = 16,
    PROTECTBATTERY = 17,
    PROTECTINFECTION = 18,
    REWARDBLOB = 19,
    REWARDCASH = 20,
    BATTERYDRAIN = 21,
    CORRUPTIONATTACK = 22,
    INFECTIONDAMAGE = 23,
    KNOCKBACK = 24,
    FREEDOM = 25,
    PHOENIX_GROUP = 26,
    RECALL = 27,
    RECALL_GROUP = 28,
    RETROROCKET_SELF = 29,
    BLOODSUCKING = 30,
    BOUNDINGBALL = 31,
    INVULNERABLE = 32,
    NANOSTIMPAK = 33,
    RETURNHOMEHEAL = 34,
    BUFFHEAL = 35,
    EXTRABANK = 36,
    CORRUPTIONATTACKWIN = 38,
    CORRUPTIONATTACKLOSE = 39,
};

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
    PLAYERS = 2,
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
        assert(len <= MAX_SKILLRESULT_SIZE);
        size = len;
        memcpy(payload, dat, len);
    }
    SkillResult() {
        size = 0;
    }
};

struct SkillData {
    SkillType skillType; // eST
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

    void useNanoSkill(CNSocket*, SkillData*, sNano&, std::vector<ICombatant*>);
    void useNPCSkill(EntityRef, int skillID, std::vector<ICombatant*>);

    std::vector<ICombatant*> matchTargets(ICombatant*, SkillData*, int, int32_t*);
    int getCSTBFromST(SkillType skillType);
}
