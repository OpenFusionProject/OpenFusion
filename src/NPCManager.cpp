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
            BaseNPC tmp(npc.value()["x"], npc.value()["y"], npc.value()["z"], npc.value()["id"]);

            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;

            NPCs[tmp.appearanceData.iNPC_ID] = tmp;

            if (npc.value()["id"] == 641 || npc.value()["id"] == 642)
                RespawnPoints.push_back({ npc.value()["x"], npc.value()["y"], ((int)npc.value()["z"]) + RESURRECT_HEIGHT });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    // load temporary mob dump
    try {
        std::ifstream inFile(settings::MOBJSON); // not in settings, since it's temp
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["iX"], npc.value()["iY"], npc.value()["iZ"], npc.value()["iNPCType"],
                npc.value()["iHP"], npc.value()["iConditionBitFlag"], npc.value()["iAngle"], npc.value()["iBarkerType"]);

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
            WarpLocation warpLoc = { warp.value()["m_iToX"], warp.value()["m_iToY"], warp.value()["m_iToZ"] };
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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_START, npcVendorStart);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE, npcVendorTable);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_ITEM_BUY, npcVendorBuy);
}

void NPCManager::npcVendorBuy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY* req = (sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    int itemCost = 100; //TODO: placeholder, look up the price of item

    //TODO: need to check if user has enough money + free inventory space, else send fail packet 

    if (itemCost > plr.plr->money) {
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL, sizeof(P_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL));
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC, resp);

    plr.plr->money = plr.plr->money - itemCost;
    plr.plr->Inven[resp.iInvenSlotNum] = req->Item;

    resp.iCandy = plr.plr->money;
    resp.iInvenSlotNum = req->iInvenSlotNum;
    resp.Item = req->Item;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC));
}

void NPCManager::npcVendorTable(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE* req = (sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC, resp);

    //TODO: data needs to be read from shopkeeper tabledata
    //check req->iVendorID and req->iNPC_ID
    
    //Exaple Items
    //shirt
    //--------------------------------------------------------------
    sItemBase base1;
    base1.iID = 60;
    //??
    base1.iOpt = 0;
    //expire date
    base1.iTimeLimit = -1;
    base1.iType = 1;

    sItemVendor item1;
    item1.item = base1;
    //??
    item1.iSortNum = 0;
    item1.iVendorID = 1;
    //cost amount in float? (doesn't work rn, need to figure out)
    item1.fBuyCost = 100.0;
    //--------------------------------------------------------------

    //pants
    //--------------------------------------------------------------
    sItemBase base2;
    base2.iID = 61;
    base2.iOpt = 0;
    base2.iTimeLimit = -1;
    base2.iType = 2;

    sItemVendor item2;
    item2.item = base2;
    item2.iSortNum = 1;
    item2.iVendorID = 1;
    item2.fBuyCost = 250.0;
    //--------------------------------------------------------------

    //shoes
    //--------------------------------------------------------------
    sItemBase base3;
    base3.iID = 51;
    base3.iOpt = 0;
    base3.iTimeLimit = -1;
    base3.iType = 3;

    sItemVendor item3;
    item3.item = base3;
    item3.iSortNum = 2;
    item3.iVendorID = 1;
    item3.fBuyCost = 350.0;
    //--------------------------------------------------------------

    resp.item[0] = item1;
    resp.item[1] = item2;
    resp.item[2] = item3;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC));
}

void NPCManager::npcVendorStart(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_START))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_START* req = (sP_CL2FE_REQ_PC_VENDOR_START*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_START_SUCC, resp);

    resp.iNPC_ID = req->iNPC_ID;
    resp.iVendorID = req->iVendorID;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_START_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_START_SUCC));
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
