#pragma once

#include <set>
#include <vector>

#include "CNShardServer.hpp"

typedef void (*ActivePowerHandler)(CNSocket*, CNPacketData*, int16_t, int16_t, int16_t, int32_t, int32_t);

struct NanoPower {
    std::set<int> Powers;
    ActivePowerHandler handler;
    int16_t skillId;
    int16_t eSkillType;
    int32_t flag;
    int32_t amount;

    NanoPower(std::set<int> p, ActivePowerHandler h, int16_t id, int16_t t, int32_t f, int32_t a) : Powers(p), handler(h), skillId(id), eSkillType(t), flag(f), amount(a) {}

    void handle(CNSocket *sock, CNPacketData *data, int16_t nanoId) {
        if (handler == nullptr)
            return;

        handler(sock, data, nanoId, skillId, eSkillType, flag, amount);
    }
};

namespace NanoManager {
    //extern std::set<int> StunPowers, HealPowers, RecallPowers, DrainPowers, SnarePowers, DamagePowers, GroupRevivePowers, LeechPowers, SleepPowers;
    //extern std::vector<std::set<int>> PowerSets;

    void init();
    void nanoSummonHandler(CNSocket* sock, CNPacketData* data);
    void nanoEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoUnEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoGMGiveHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillUseHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data);
    void nanoRecallHandler(CNSocket* sock, CNPacketData* data);

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoId, int16_t slot);
    void summonNano(CNSocket* sock, int slot);
    void setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId);
    void resetNanoSkill(CNSocket* sock, int16_t nanoId);
    
    void nanoDebuff(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t eSkillType, int32_t iCBFlag, int32_t damageAmount = 0);

    void nanoHeal(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t eSkillType, int32_t flag, int16_t healAmount);
    void nanoDamage(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t eSkillType, int32_t flag, int32_t damageAmount);
    void nanoLeech(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t eSkillType, int32_t flag, int32_t leechAmount);

    void nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0);
    void nanoUnbuff(CNSocket* sock, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0);
}
