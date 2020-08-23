#include "NPCManager.hpp"
#include "settings.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>

#include "contrib/JSON.hpp"

std::map<int32_t, BaseNPC> NPCManager::NPCs;

void NPCManager::init() {
    // load NPCs from NPCs.json into our NPC manager

    try {
        std::ifstream inFile("NPCs.json");
        nlohmann::json jsonData;

        // read file into jsonData
        inFile >> jsonData;

        for (auto& npc : jsonData) {
            BaseNPC tmp(npc["x"], npc["y"], npc["z"], npc["id"]);
            NPCManager::NPCs[tmp.appearanceData.iNPC_ID] = tmp;
        }

        std::cout << "populated " << NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPC.json file! Reason:" << std::endl << err.what() << std::endl;
    }
        REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpManager);
}

#undef CHECKNPC

void NPCManager::updatePlayerNPCS(CNSocket* sock, PlayerView& view) {
    std::list<int32_t> yesView;
    std::list<int32_t> noView;

    for (auto& pair : NPCs) {
        int diffX = abs(view.plr.x - pair.second.appearanceData.iX);
        int diffY = abs(view.plr.y - pair.second.appearanceData.iY);

        if (diffX < settings::VIEWDISTANCE && diffY < settings::VIEWDISTANCE) {
            yesView.push_back(pair.first);
        } else {
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

        ++i;
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
void NPCManager::npcWarpManager(CNSocket* sock, CNPacketData* data)
{
    std::ifstream warp_file("warps.json", std::ifstream::binary);
    nlohmann::json warp;
    warp_file >> warp;

    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    INITSTRUCT(sP_CL2FE_REQ_PC_WARP_USE_NPC,warpNpc);
    //Send to Client
    sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC* resp = (sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
    resp->iX = warp[std::to_string(warpNpc->iWarpID)]["m_iToX"];
    resp->iY = warp[std::to_string(warpNpc->iWarpID)]["m_iToY"];
    resp->iZ = warp[std::to_string(warpNpc->iWarpID)]["m_iToZ"];
    //Add Instance Stuff Later
    std::cerr << "OpenFusion: Warp using " << "Warp ID" <<warpNpc->iWarpID << "Warp to Z:"<< warp[std::to_string(warpNpc->iWarpID)]["m_iToZ"] << std::endl;

    sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC), sock->getFEKey()));

}
