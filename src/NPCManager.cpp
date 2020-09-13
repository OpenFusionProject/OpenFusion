#include "NPCManager.hpp"
#include "settings.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>

#include "contrib/JSON.hpp"

std::map<int32_t, BaseNPC> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;

void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);
}

void NPCManager::updatePlayerNPCS(CNSocket* sock, PlayerView& view) {
    std::list<int32_t> yesView;
    std::list<int32_t> noView;

    for (auto& pair : NPCs) {
        int diffX = abs(view.plr->x - pair.second.appearanceData.iX);
        int diffY = abs(view.plr->y - pair.second.appearanceData.iY);

        if (diffX < settings::NPCDISTANCE && diffY < settings::NPCDISTANCE) {
            yesView.push_back(pair.first);
        }
        else {
            noView.push_back(pair.first);
        }
    }

    INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
    std::list<int32_t>::iterator i = view.viewableNPCs.begin();
    while (i != view.viewableNPCs.end()) {
        int32_t id = *i;

        if (std::find(noView.begin(), noView.end(), id) != noView.end()) {
            // it shouldn't be visible, send NPC_EXIT

            exitData.iNPC_ID = id;
            sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));

            // remove from view
            view.viewableNPCs.erase(i++);
        }
        else {
            i++;
        }
    }

    INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
    for (int32_t id : yesView) {
        if (std::find(view.viewableNPCs.begin(), view.viewableNPCs.end(), id) == view.viewableNPCs.end()) {
            // needs to be added to viewableNPCs! send NPC_ENTER

            enterData.NPCAppearanceData = NPCs[id].appearanceData;
            sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));

            // add to viewable
            view.viewableNPCs.push_back(id);
        }
    }

    PlayerManager::players[sock].viewableNPCs = view.viewableNPCs;
}

void NPCManager::npcBarkHandler(CNSocket* sock, CNPacketData* data) {} // stubbed for now

void NPCManager::npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    INITSTRUCT(sP_FE2CL_NPC_ENTER, resp);
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    if (!plr->IsGM || req->iNPCType >= 3314)
        return;

    resp.NPCAppearanceData.iNPC_ID = rand(); // cpunch-style
    resp.NPCAppearanceData.iNPCType = req->iNPCType;
    resp.NPCAppearanceData.iHP = 1000; // TODO: placeholder
    resp.NPCAppearanceData.iX = plr->x;
    resp.NPCAppearanceData.iY = plr->y;
    resp.NPCAppearanceData.iZ = plr->z;

    sock->sendPacket((void*)&resp, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
    for (CNSocket *s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&resp, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
}

void NPCManager::npcWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_NPC* warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;
    PlayerView& plrv = PlayerManager::players[sock];

    // sanity check
    if (Warps.find(warpNpc->iWarpID) == Warps.end())
        return;

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp);
    resp.iX = Warps[warpNpc->iWarpID].x;
    resp.iY = Warps[warpNpc->iWarpID].y;
    resp.iZ = Warps[warpNpc->iWarpID].z;

    // force player & NPC reload
    plrv.viewable.clear();
    plrv.viewableNPCs.clear();

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
}
