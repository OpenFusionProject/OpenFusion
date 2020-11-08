#pragma once

#include <set>
#include <vector>

#include "CNShardServer.hpp"

struct TargetData {
    int targetID[4];
};

typedef void (*PowerHandler)(CNSocket*, TargetData, int16_t, int16_t, int16_t, int16_t, int16_t, int32_t, int16_t);

struct ActivePower {
    int16_t skillType;
    int32_t bitFlag;
    int16_t timeBuffID;
    PowerHandler handler;

    ActivePower(int16_t s, int32_t b, int16_t t, PowerHandler h) : skillType(s), bitFlag(b), timeBuffID(t), handler(h) {}

    void handle(CNSocket *sock, TargetData targetData, int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount) {
        if (handler == nullptr)
            return;

        handler(sock, targetData, nanoID, skillID, duration, amount, skillType, bitFlag, timeBuffID);
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

struct NanoTuning {
    int reqItemCount;
    int reqItems;
};

struct SkillData {
    int skillType;
    int targetType;
    int drainType;
    int batteryUse[4];
    int durationTime[4];
    int powerIntensity[4];
};

namespace NanoManager {
    extern std::vector<ActivePower> ActivePowers;
    extern std::vector<PassivePower> PassivePowers;
    extern std::map<int32_t, NanoData> NanoTable;
    extern std::map<int32_t, NanoTuning> NanoTunings;
    extern std::map<int32_t, SkillData> SkillTable;
    void init();

    void nanoSummonHandler(CNSocket* sock, CNPacketData* data);
    void nanoEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoUnEquipHandler(CNSocket* sock, CNPacketData* data);
    void nanoGMGiveHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillUseHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetHandler(CNSocket* sock, CNPacketData* data);
    void nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data);
    void nanoRecallRegisterHandler(CNSocket* sock, CNPacketData* data);
    void nanoRecallHandler(CNSocket* sock, CNPacketData* data);
    void nanoPotionHandler(CNSocket* sock, CNPacketData* data);

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm=false);
    void summonNano(CNSocket* sock, int slot);
    void setNanoSkill(CNSocket* sock, sP_CL2FE_REQ_NANO_TUNE* skill);
    void resetNanoSkill(CNSocket* sock, int16_t nanoID);
    void nanoUnbuff(CNSocket* sock, TargetData targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower);
    void applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags);
    int nanoStyle(int nanoID);
}
