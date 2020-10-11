#pragma once

#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPC.hpp"

#include "contrib/JSON.hpp"

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
    extern nlohmann::json NPCData;
    extern int32_t nextId;
    void init();

    void addNPC(std::vector<Chunk*> viewableChunks, int32_t id);
    void removeNPC(std::vector<Chunk*> viewableChunks, int32_t id);
    void destroyNPC(int32_t);
    void updateNPCPosition(int32_t, int X, int Y, int Z, int angle);
    void updateNPCPosition(int32_t, int X, int Y, int Z);

    void sendToViewable(BaseNPC* npc, void* buf, uint32_t type, size_t size);

    void npcBarkHandler(CNSocket* sock, CNPacketData* data);
    void npcSummonHandler(CNSocket* sock, CNPacketData* data);
    void npcUnsummonHandler(CNSocket* sock, CNPacketData* data);
    void npcWarpHandler(CNSocket* sock, CNPacketData* data);
    void npcWarpTimeMachine(CNSocket* sock, CNPacketData* data);

    void npcVendorStart(CNSocket* sock, CNPacketData* data);
    void npcVendorTable(CNSocket* sock, CNPacketData* data);
    void npcVendorBuy(CNSocket* sock, CNPacketData* data);
    void npcVendorSell(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyback(CNSocket* sock, CNPacketData* data);
    void npcVendorBuyBattery(CNSocket* sock, CNPacketData* data);
    void npcCombineItems(CNSocket* sock, CNPacketData* data);

    void handleWarp(CNSocket* sock, int32_t warpId);

    BaseNPC* getNearestNPC(std::vector<Chunk*> chunks, int X, int Y, int Z);
}
