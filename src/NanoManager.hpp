#pragma once

#include <set>
#include <vector>

#include "CNShardServer.hpp"

typedef void (*ActivePowerHandler)(CNSocket*, CNPacketData*, int16_t, int16_t, int16_t, int32_t, int32_t);

struct ActivePower {
    std::set<int> powers;
    ActivePowerHandler handler;
    int16_t eSkillType;
    int32_t flag;
    int32_t amount;

    ActivePower(std::set<int> p, ActivePowerHandler h, int16_t t, int32_t f, int32_t a) : powers(p), handler(h), eSkillType(t), flag(f), amount(a) {}

    void handle(CNSocket *sock, CNPacketData *data, int16_t nanoId, int16_t skillId) {
        if (handler == nullptr)
            return;

        handler(sock, data, nanoId, skillId, eSkillType, flag, amount);
    }
};

struct PassivePower {
    std::set<int> powers;
    int16_t eSkillType;
    int32_t iCBFlag;
    int16_t eCharStatusTimeBuffID;
    int16_t iValue;
    bool groupPower;

    PassivePower(std::set<int> p, int16_t t, int32_t f, int16_t b, int16_t a, bool g) : powers(p), eSkillType(t), iCBFlag(f), eCharStatusTimeBuffID(b), iValue(a), groupPower(g) {}
};

struct NanoData {
    int style;
};

namespace NanoManager {
    extern std::vector<ActivePower> ActivePowers;
    extern std::vector<PassivePower> PassivePowers;
    extern std::map<int32_t, NanoData> NanoTable;
    void init();

    void nanoSummonHandler(CNSocket* sock, CNPacketData* data);
    void nanoEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoUnEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoGMGiveHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillUseHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data);
    void nanoRecallHandler(CNSocket* sock, CNPacketData* data);
    void nanoPotionHandler(CNSocket* sock, CNPacketData* data);

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoId, int16_t slot, bool spendfm=false);
    void summonNano(CNSocket* sock, int slot);
    void setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId);
    void resetNanoSkill(CNSocket* sock, int16_t nanoId);

    void nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0, bool groupPower = false);
    void nanoUnbuff(CNSocket* sock, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0, bool groupPower = false);

    int nanoStyle(int nanoId);
    void revivePlayer(Player* plr);
    void nanoChangeBuff(CNSocket* sock, Player* plr, int32_t cbFrom, int32_t cbTo);
}
