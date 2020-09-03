#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
#include "NPCManager.hpp"

namespace CombatManager {
    void init();

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);

    void giveReward(CNSocket *sock);
    
    //helper methods
    void combatBegin(CNSocket* sock, BaseNPC& mob);
}
