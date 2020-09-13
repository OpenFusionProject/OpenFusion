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

    // Helper methods
    void addNano(CNSocket* sock, int16_t nanoId, int16_t slot);
    void summonNano(CNSocket* sock, int slot);
    void setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId);
    void resetNanoSkill(CNSocket* sock, int16_t nanoId);
}
