#pragma once

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include <map>
#include <vector>

#define RESURRECT_HEIGHT 400

// this should really be called vec3 or something...
struct WarpLocation {
    int x, y, z, mapNum, isInstance;
};

namespace NPCManager {
    extern std::map<int32_t, BaseNPC> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;

    extern std::vector<WarpLocation> RespawnPoints;
    void init();

    void npcBarkHandler(CNSocket* sock, CNPacketData* data);
    void updatePlayerNPCS(CNSocket* sock, PlayerView& plr);
    void npcWarpHandler(CNSocket* sock, CNPacketData* data);
    void npcSummonHandler(CNSocket* sock, CNPacketData* data);
    void changeNPCMAP(CNSocket* sock, PlayerView& view, int mapNum);
    void reloadNPCs();
    void SummonWrite(CNSocket* sock, PlayerView& view, int NPCID);
    void unSummonWrite(CNSocket* sock, PlayerView& view);


}
