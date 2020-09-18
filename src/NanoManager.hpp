#pragma once

#include "CNShardServer.hpp"

namespace NanoManager {
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
    void nanoHeal(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t healAmount);
    void nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0);
    void nanoUnbuff(CNSocket* sock, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue = 0);
    void nanoDamage(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int32_t damageAmount);
    void nanoLeech(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int32_t leechAmount);
}
