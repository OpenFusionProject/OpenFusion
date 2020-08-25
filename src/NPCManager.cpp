#include "NPCManager.hpp"
#include "settings.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>

#include "contrib/JSON.hpp"

std::map<int32_t, BaseNPC> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;

void NPCManager::init() {
    // load NPCs from NPCs.json into our NPC manager

    try {
        std::ifstream inFile("NPCs.json");
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["x"], npc.value()["y"], npc.value()["z"], npc.value()["id"]);
            NPCs[tmp.appearanceData.iNPC_ID] = tmp;
        }

        std::cout << "[INFO] populated " << NPCs.size() << " NPCs" << std::endl;
    } catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    try {
        std::ifstream infile("warps.json");
        nlohmann::json warpData;

        // read file into json
        infile >> warpData;

        for (nlohmann::json::iterator warp = warpData.begin(); warp != warpData.end(); warp++) {
            WarpLocation warpLoc = {warp.value()["m_iToX"], warp.value()["m_iToY"], warp.value()["m_iToZ"]};
            int warpID = atoi(warp.key().c_str());
            Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] populated " << Warps.size() << " Warps" << std::endl;
    } catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed warps.json file! Reason:" << err.what() << std::endl;
    }


    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpManager);
}

void NPCManager::updatePlayerNPCS(CNSocket* sock, PlayerView& view) {
    std::list<int32_t> yesView;
    std::list<int32_t> noView;

    for (auto& pair : NPCs) {
        int diffX = abs(view.plr->x - pair.second.appearanceData.iX);
        int diffY = abs(view.plr->y - pair.second.appearanceData.iY);

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
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_NPC* warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;

    // sanity check
    if (Warps.find(warpNpc->iWarpID) == Warps.end())
        return;

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp);
    resp.iX = Warps[warpNpc->iWarpID].x;
    resp.iY = Warps[warpNpc->iWarpID].y;
    resp.iZ = Warps[warpNpc->iWarpID].z;
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));

}
