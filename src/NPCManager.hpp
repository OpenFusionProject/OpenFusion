#ifndef _NPCMANAGER_HPP
#define _NPCMANAGER_HPP

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include <map>

namespace NPCManager {
    extern std::map<int32_t, BaseNPC> NPCs;
    void init();

    void updatePlayerNPCS(CNSocket* sock, PlayerView& plr);
    void npcWarpManager(CNSocket* sock, CNPacketData* data);

}

#endif
