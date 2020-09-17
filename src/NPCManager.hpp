#pragma once

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include <map>
#include <vector>

#define RESURRECT_HEIGHT 400

// this should really be called vec3 or something...
struct WarpLocation {
    int x, y, z;
};

namespace NPCManager {
    extern std::map<int32_t, BaseNPC*> NPCs;
    extern std::map<int32_t, WarpLocation> Warps;
    extern std::vector<WarpLocation> RespawnPoints;
    void init();

    void addNPC(std::vector<Chunk*> viewableChunks, int32_t);
    void removeNPC(std::vector<Chunk*> viewableChunks, int32_t);

    void npcBarkHandler(CNSocket* sock, CNPacketData* data);
    void npcSummonHandler(CNSocket* sock, CNPacketData* data);
    void npcWarpHandler(CNSocket* sock, CNPacketData* data);

    void npcVendorStart(CNSocket* sock, CNPacketData* data);
    void npcVendorTable(CNSocket* sock, CNPacketData* data);
    void npcVendorBuy(CNSocket* sock, CNPacketData* data);
    void npcVendorSell(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyback(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyBattery(CNSocket* sock, CNPacketData* data);
}
