#ifndef _NPCMANAGER_HPP
#define _NPCMANAGER_HPP

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include <map>

struct WarpLocation {
    int x, y, z;
};

namespace NPCManager {
    extern std::map<int32_t, BaseNPC> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;
    void init();

    void updatePlayerNPCS(CNSocket* sock, PlayerView& plr);
    void npcWarpManager(CNSocket* sock, CNPacketData* data);

}

#endif
