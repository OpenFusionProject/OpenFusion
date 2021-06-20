#pragma once

#include "core/Core.hpp"
#include "Combat.hpp"

typedef void (*PowerHandler)(CNSocket*, std::vector<int>, int16_t, int16_t, int16_t, int16_t, int16_t, int32_t, int16_t);

struct Power {
    int16_t skillType;
    int32_t bitFlag;
    int16_t timeBuffID;
    PowerHandler handler;

    Power(int16_t s, int32_t b, int16_t t, PowerHandler h) : skillType(s), bitFlag(b), timeBuffID(t), handler(h) {}

    void handle(CNSocket *sock, std::vector<int> targetData, int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount) {
        if (handler == nullptr)
            return;

        handler(sock, targetData, nanoID, skillID, duration, amount, skillType, bitFlag, timeBuffID);
    }
};

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
    extern std::vector<Power> Powers;
    extern std::map<int32_t, SkillData> SkillTable;

    void removeBuff(CNSocket* sock, std::vector<int> targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower);
    int applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags);

    std::vector<int> findTargets(Player* plr, int skillID, CNPacketData* data = nullptr);
}
