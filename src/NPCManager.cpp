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
    // load NPCs from NPCs.json into our NPC manager

    // Temporary fix, IDs will be pulled from json later
    int i = 0;
    
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {

            BaseNPC tmp(npc.value()["x"], npc.value()["y"], npc.value()["z"], npc.value()["id"], npc.value()["mapNum"], std::stoi(npc.key()));
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;


            NPCs[tmp.appearanceData.iNPC_ID] = tmp;

            if (npc.value()["id"] == 641 || npc.value()["id"] == 642)
                RespawnPoints.push_back({ npc.value()["x"], npc.value()["y"], ((int)npc.value()["z"]) + RESURRECT_HEIGHT,(int)npc.value()["mapNum"] });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    // load temporary mob dump
    try {
        std::ifstream inFile("data/mobs.json"); // not in settings, since it's temp
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["iX"], npc.value()["iY"], npc.value()["iZ"], npc.value()["iNPCType"],

            npc.value()["iHP"], npc.value()["iConditionBitFlag"], npc.value()["iAngle"], npc.value()["iBarkerType"], npc.value()["mapNum"], std::stoi(npc.key()));
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;


            NPCs[tmp.appearanceData.iNPC_ID] = tmp;
        }

        std::cout << "[INFO] populated " << NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
    }

    try {
        std::ifstream infile(settings::WARPJSON);
        nlohmann::json warpData;

        // read file into json
        infile >> warpData;

        for (nlohmann::json::iterator warp = warpData.begin(); warp != warpData.end(); warp++) {
            WarpLocation warpLoc = { warp.value()["m_iToX"], warp.value()["m_iToY"], warp.value()["m_iToZ"],warp.value()["m_iToMapNum"],warp.value()["m_iIsInstance"],warp.value()["m_iLimit_TaskID"],warp.value()["m_iNpcNumber"] };
            int warpID = atoi(warp.key().c_str());
            Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] populated " << Warps.size() << " Warps" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed warps.json file! Reason:" << err.what() << std::endl;
    }


    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);
}

void NPCManager::updatePlayerNPCS(CNSocket* sock, PlayerView& view) {
    std::list<int32_t> yesView;
    std::list<int32_t> noView;

    for (auto& pair : NPCs) {
        if (view.plr->mapNum == pair.second.imapNum)
        {
            int diffX = abs(view.plr->x - pair.second.appearanceData.iX);
            int diffY = abs(view.plr->y - pair.second.appearanceData.iY);

            if (diffX < settings::NPCDISTANCE && diffY < settings::NPCDISTANCE) {
                yesView.push_back(pair.first);
            }
            else {
                noView.push_back(pair.first);
            }
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

void NPCManager::npcBarkHandler(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_BARKER* bark = (sP_CL2FE_REQ_BARKER*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    INITSTRUCT(sP_FE2CL_REP_BARKER, resp);
    resp.iMissionStringID = bark->iMissionTaskID;
    resp.iNPC_ID = bark->iNPC_ID;

    // Send bark to other players.
    for (CNSocket* otherSock : plr.viewable) {
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_BARKER, sizeof(sP_FE2CL_REP_BARKER));
    }

    // Then ourself.
    sock->sendPacket((void*)&resp, P_FE2CL_REP_BARKER, sizeof(sP_FE2CL_REP_BARKER));
}

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
    INITSTRUCT(sP_FE2CL_INSTANCE_MAP_INFO, instanceInfo);

    std::cerr << "Warped to Map Num:" << Warps[warpNpc->iWarpID].mapNum <<" NPC ID "<< Warps[warpNpc->iWarpID].npcID << std::endl;
    if (Warps[warpNpc->iWarpID].isInstance)
    {
        INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, response);
        if (Warps[warpNpc->iWarpID].limitTaskID != 0 || (Warps[warpNpc->iWarpID].npcID >= 3123 && Warps[warpNpc->iWarpID].npcID <= 3132))
        {
            plrv.plr->mapUID = (Warps[warpNpc->iWarpID].limitTaskID + 1) * (plrv.plr->iID + 1);
            std::cout << "Enter a private instance " <<"UID: "<< plrv.plr->mapUID << std::endl;

        }
        else
        {
            plrv.plr->mapUID = 0;
        }
        response.iX = Warps[warpNpc->iWarpID].x;
        response.iY = Warps[warpNpc->iWarpID].y;
        response.iZ = Warps[warpNpc->iWarpID].z;
        instanceInfo.iInstanceMapNum = Warps[warpNpc->iWarpID].mapNum;
        plrv.plr->mapNum = Warps[warpNpc->iWarpID].mapNum;
        plrv.viewable.clear();
        plrv.viewableNPCs.clear();
        sock->sendPacket((void*)&instanceInfo, P_FE2CL_INSTANCE_MAP_INFO, sizeof(sP_FE2CL_INSTANCE_MAP_INFO));
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); //Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpNpc->iWarpID].x;
        resp.iY = Warps[warpNpc->iWarpID].y;
        resp.iZ = Warps[warpNpc->iWarpID].z;
        plrv.viewable.clear();
        plrv.viewableNPCs.clear();
        plrv.plr->mapNum = 0;
        plrv.plr->mapUID = 0;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
    }
}
void NPCManager::changeNPCMAP(CNSocket* sock, PlayerView& view, int mapNum) {
    std::ifstream inFile(settings::NPCJSON);
    nlohmann::json npcData;
    // read file into json
    inFile >> npcData;
    inFile.close();
   
    for (auto& pair : NPCs) {
        if (view.plr->mapNum == pair.second.imapNum && pair.second.appearanceData.iNPCType == npcData[std::to_string(pair.second.jsonRef)]["id"])
        {
            //std::cout << "aaa"  << std::endl;
            int diffX = abs(view.plr->x - pair.second.appearanceData.iX);
            int diffY = abs(view.plr->y - pair.second.appearanceData.iY);

            if (diffX < 100 && diffY < 100)
            {
                std::cout <<"npcId"<< pair.second.appearanceData.iNPCType << " ChangeNpcMap Map ID: " << mapNum << "NPCID: " << pair.second.jsonRef << "Name" << npcData[std::to_string(pair.second.jsonRef)]["name"] << std::endl;
                npcData[std::to_string(pair.second.jsonRef)]["mapNum"] = (mapNum);
                std::cout << npcData[std::to_string(pair.second.jsonRef)]["mapNum"] << std::endl;
                std::cout << npcData[std::to_string(pair.second.jsonRef)] << std::endl;
                //remove((settings::NPCJSON).data());
                std::ofstream outFile(settings::NPCJSON);
                outFile << npcData << std::endl;
                outFile.close();
                reloadNPCs();
                return;
            }

        }

    }

}
void NPCManager::reloadNPCs()
{
    // load NPCs from NPCs.json into our NPC manager
    int i = 0;
    NPCs.clear();
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["x"], npc.value()["y"], npc.value()["z"], npc.value()["id"], npc.value()["mapNum"], std::stoi(npc.key()));
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;

            NPCs[tmp.appearanceData.iNPC_ID] = tmp;

            if (npc.value()["id"] == 641 || npc.value()["id"] == 642)
                RespawnPoints.push_back({ npc.value()["x"], npc.value()["y"], ((int)npc.value()["z"]) + RESURRECT_HEIGHT,(int)npc.value()["mapNum"] });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    // load temporary mob dump
    try {
        std::ifstream inFile("data/mobs.json"); // not in settings, since it's temp
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["iX"], npc.value()["iY"], npc.value()["iZ"], npc.value()["iNPCType"],
                npc.value()["iHP"], npc.value()["iConditionBitFlag"], npc.value()["iAngle"], npc.value()["iBarkerType"], npc.value()["mapNum"], std::stoi(npc.key()));
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;

            NPCs[tmp.appearanceData.iNPC_ID] = tmp;
        }

        std::cout << "[INFO] populated " << NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
    }

}
void NPCManager::SummonWrite(CNSocket* sock, PlayerView& view, int NPCID)
{
    //Unfinished Ignore
    std::cout << "SummonWrite" << std::endl;

    INITSTRUCT(sP_FE2CL_NPC_ENTER, resp);
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    //if (!plr->IsGM)
     //   return;
    std::cout << "GM" << std::endl;


    //"554":{"id":1618,"mapNum":75,"name":"Location A292","x":130230.99365234375,"y":227686.9873046875,"z":-18806.99920654297}
    resp.NPCAppearanceData.iNPC_ID = rand(); // cpunch-style
    resp.NPCAppearanceData.iNPCType = NPCID;
    resp.NPCAppearanceData.iHP = 1000; // TODO: placeholder
    resp.NPCAppearanceData.iX = plr->x;
    resp.NPCAppearanceData.iY = plr->y;
    resp.NPCAppearanceData.iZ = plr->z;
    resp.NPCAppearanceData.iAngle = plr->angle;
    std::string newNpc;
    //resp.NPCAppearanceData
    std::ifstream inFile(settings::NPCJSON);
    nlohmann::json npcData;
    // read file into json
    inFile >> npcData;
    inFile.close();
    std::string validId;
    int i = 0;
    for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
        i++;
        if (npcData[std::to_string(i)]["name"] == "")
        {
            validId = std::to_string(i);
            //npcData[validId]["id"] = NPCID;
            //newNpc = "{\"id\":" + std::to_string(NPCID) + ",\"mapNum\":" + std::to_string(plr->mapNum) + ",\"name\":\"Temp\",\"x\":" + std::to_string(resp.NPCAppearanceData.iX) + ",\"y\":" + std::to_string(resp.NPCAppearanceData.iY) + ",\"z\":" + std::to_string(resp.NPCAppearanceData.iZ)+"}";

            npcData[validId]["id"] = NPCID;
            npcData[validId]["mapNum"] = plr->mapNum;
            npcData[validId]["name"] = "Temp";
            npcData[validId]["x"] = resp.NPCAppearanceData.iX;
            npcData[validId]["y"] = resp.NPCAppearanceData.iY;
            npcData[validId]["z"] = resp.NPCAppearanceData.iZ;
            std::cout << npcData[validId] << std::endl;

            std::ofstream outFile(settings::NPCJSON);
            outFile << npcData << std::endl;
            outFile.close();
            reloadNPCs();
            std::cout << "SendPacket" << std::endl;
            sock->sendPacket((void*)&resp, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
            return;
        }
    }
    
}
void NPCManager::unSummonWrite(CNSocket* sock, PlayerView& view) {
    std::ifstream inFile(settings::NPCJSON);
    nlohmann::json npcData;
    // read file into json
    inFile >> npcData;
    inFile.close();
    for (auto& pair : NPCs) {
        if (view.plr->mapNum == pair.second.imapNum && pair.second.appearanceData.iNPCType == npcData[std::to_string(pair.second.jsonRef)]["id"])
        {
            //std::cout << "aaa"  << std::endl;
            int diffX = abs(view.plr->x - pair.second.appearanceData.iX);
            int diffY = abs(view.plr->y - pair.second.appearanceData.iY);

            if (diffX < 100 && diffY < 100)
            {
                std::cout << std::to_string(pair.second.jsonRef) << npcData[std::to_string(pair.second.jsonRef)] << std::endl;
                npcData[std::to_string(pair.second.jsonRef)]["id"] = 0;
                npcData[std::to_string(pair.second.jsonRef)]["mapNum"] = 0;
                npcData[std::to_string(pair.second.jsonRef)]["name"] = "";
                npcData[std::to_string(pair.second.jsonRef)]["x"] = 0;
                npcData[std::to_string(pair.second.jsonRef)]["y"] = 0;
                npcData[std::to_string(pair.second.jsonRef)]["z"] = 0;
                //remove((settings::NPCJSON).data());
                std::ofstream outFile(settings::NPCJSON);
                outFile << npcData << std::endl;
                outFile.close();
                reloadNPCs();
                return;
            }

        }

    }

}

