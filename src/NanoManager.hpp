#pragma once

#include <set>
#include <vector>

#include "Player.hpp"
#include "CNShardServer.hpp"

struct NanoData {
    int style;
};

struct NanoTuning {
    int reqItemCount;
    int reqItems;
};

namespace NanoManager {
    extern std::map<int32_t, NanoData> NanoTable;
    extern std::map<int32_t, NanoTuning> NanoTunings;
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
    void summonNano(CNSocket* sock, int slot, bool silent = false);
    void setNanoSkill(CNSocket* sock, sP_CL2FE_REQ_NANO_TUNE* skill);
    void resetNanoSkill(CNSocket* sock, int16_t nanoID);
    int nanoStyle(int nanoID);
    std::vector<int> findTargets(Player* plr, int skillID, CNPacketData* data = nullptr);
    bool getNanoBoost(Player* plr);
}
