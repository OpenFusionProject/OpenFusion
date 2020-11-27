#pragma once

#include <set>
#include <vector>

#include "CNShardServer.hpp"

typedef void (*PowerHandler)(CNSocket*, std::vector<int>, int16_t, int16_t, int16_t, int16_t, int16_t, int32_t, int16_t);

struct NanoPower {
    int16_t skillType;
    int32_t bitFlag;
    int16_t timeBuffID;
    PowerHandler handler;

    NanoPower(int16_t s, int32_t b, int16_t t, PowerHandler h) : skillType(s), bitFlag(b), timeBuffID(t), handler(h) {}

    void handle(CNSocket *sock, std::vector<int> targetData, int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount) {
        if (handler == nullptr)
            return;

        handler(sock, targetData, nanoID, skillID, duration, amount, skillType, bitFlag, timeBuffID);
    }
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
    int effectArea;
    int batteryUse[4];
    int durationTime[4];
    int powerIntensity[4];
};

namespace NanoManager {
    extern std::vector<NanoPower> NanoPowers;
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
    void nanoUnbuff(CNSocket* sock, std::vector<int> targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower);
    int applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags);
    int nanoStyle(int nanoID);
    std::vector<int> findTargets(Player* plr, int skillID, CNPacketData* data = nullptr);
    bool getNanoBoost(Player* plr);
}
