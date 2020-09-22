#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>

#include "contrib/JSON.hpp"

std::map<int32_t, BaseNPC*> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;


void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_UNSUMMON, npcUnsummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_START, npcVendorStart);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE, npcVendorTable);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_ITEM_BUY, npcVendorBuy);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_ITEM_SELL, npcVendorSell);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY, npcVendorBuyback);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VENDOR_BATTERY_BUY, npcVendorBuyBattery);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_COMBINATION, npcCombineItems);
}

void NPCManager::addNPC(std::vector<Chunk*> viewableChunks, int32_t id) {
    BaseNPC* npc = NPCs[id];

    // create struct
    INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
    enterData.NPCAppearanceData = npc->appearanceData;

    for (Chunk* chunk : viewableChunks) {
        for (CNSocket* sock : chunk->players) {
            // send to socket
            sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
        }
    }
}

void NPCManager::removeNPC(int32_t id) {
    // sanity check
    if (NPCs.find(id) == NPCs.end()) {
        std::cout << "npc not found : " << id << std::endl;
        return;
    }

    BaseNPC* entity = NPCs[id];

    std::pair<int, int> pos = ChunkManager::grabChunk(entity->appearanceData.iX, entity->appearanceData.iY);

    // sanity check
    if (ChunkManager::chunks.find(pos) == ChunkManager::chunks.end()) {
        std::cout << "chunk not found!" << std::endl;
        return;
    }

    // remove NPC from the chunk
    Chunk* chunk = ChunkManager::chunks[pos];
    chunk->NPCs.erase(id);
    
    // create struct
    INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
    exitData.iNPC_ID = id;

    // remove it from the clients
    for (Chunk* chunk : ChunkManager::grabChunks(pos)) {
        for (CNSocket* sock : chunk->players) {
            // send to socket
            sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
        }
    }

    // remove from mob list if it's a mob
    if (MobManager::Mobs.find(id) != MobManager::Mobs.end())
        MobManager::Mobs.erase(id);

    // finally, remove it from the map and free it
    NPCs.erase(id);
    delete entity;

    std::cout << "npc removed!" << std::endl;
}

void NPCManager::npcVendorBuy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY* req = (sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);
    Item* item = ItemManager::getItemData(req->Item.iID, req->Item.iType);

    if (item == nullptr) {
        std::cout << "[WARN] Item id " << req->Item.iID << " with type " << req->Item.iType << " not found (buy)" << std::endl;
        // NOTE: VENDOR_ITEM_BUY_FAIL is not actually handled client-side.
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL));
        return;
    }

    int itemCost = item->sellPrice * (item->stackSize > 1 ? req->Item.iOpt : 1);
    int slot = ItemManager::findFreeSlot(plr);
    if (itemCost > plr->money || slot == -1) {
        // NOTE: VENDOR_ITEM_BUY_FAIL is not actually handled client-side.
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL));
        return;
    }

    if (slot != req->iInvenSlotNum) {
        // possible item stacking?
        std::cout << "[WARN] Client and server disagree on bought item slot (" << req->iInvenSlotNum << " vs " << slot << ")" << std::endl;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC, resp);

    plr->money = plr->money - itemCost;
    plr->Inven[slot] = req->Item;

    resp.iCandy = plr->money;
    resp.iInvenSlotNum = slot;
    resp.Item = req->Item;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC));
}

void NPCManager::npcVendorSell(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL* req = (sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (req->iInvenSlotNum < 0 || req->iInvenSlotNum >= AINVEN_COUNT || req->iItemCnt < 0) {
        std::cout << "[WARN] Client failed to sell item in slot " << req->iInvenSlotNum << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL));
        return;
    }

    sItemBase* item = &plr->Inven[req->iInvenSlotNum];
    Item* itemData = ItemManager::getItemData(item->iID, item->iType);

    if (itemData == nullptr || !itemData->sellable) { // sanity + sellable check
        std::cout << "[WARN] Item id " << item->iID << " with type " << item->iType << " not found (sell)" << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL));
        return;
    }

    sItemBase original;
    memcpy(&original, item, sizeof(sItemBase));

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC, resp);
    
    int sellValue = itemData->sellPrice * req->iItemCnt;

    // increment taros
    plr->money = plr->money + sellValue;

    // modify item
    if (plr->Inven[req->iInvenSlotNum].iOpt - req->iItemCnt > 0) { // selling part of a stack
        item->iOpt -= req->iItemCnt;
        original.iOpt = req->iItemCnt;
    }
    else { // selling entire slot
        item->iID = 0;
        item->iOpt = 0;
        item->iType = 0;
        item->iTimeLimit = 0;
    }

    // response parameters
    resp.iInvenSlotNum = req->iInvenSlotNum;
    resp.iCandy = plr->money;
    resp.Item = original; // the item that gets sent to buyback
    resp.ItemStay = *item; // the void item that gets put in the slot

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC));
}

void NPCManager::npcVendorBuyback(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY* req = (sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Item* item = ItemManager::getItemData(req->Item.iID, req->Item.iType);

    if (item == nullptr) {
        std::cout << "[WARN] Item id " << req->Item.iID << " with type " << req->Item.iType << " not found (rebuy)" << std::endl;
        // NOTE: VENDOR_ITEM_BUY_FAIL is not actually handled client-side.
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL));
        return;
    }

    // sell price is used on rebuy. ternary identifies stacked items
    int itemCost = item->sellPrice * (item->stackSize > 1 ? req->Item.iOpt : 1);
    int slot = ItemManager::findFreeSlot(plr);
    if (itemCost > plr->money || slot == -1) {
        // NOTE: VENDOR_ITEM_BUY_FAIL is not actually handled client-side.
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL));
        return;
    }

    if (slot != req->iInvenSlotNum) {
        // possible item stacking?
        std::cout << "[WARN] Client and server disagree on bought item slot (" << req->iInvenSlotNum << " vs " << slot << ")" << std::endl;
    }

    plr->money = plr->money - itemCost;
    plr->Inven[slot] = req->Item;

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC, resp);
    // response parameters
    resp.iCandy = plr->money;
    resp.iInvenSlotNum = slot;
    resp.Item = req->Item;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC));
}

void NPCManager::npcVendorTable(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE* req = (sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE*)data->buf;
    
    if (req->iVendorID != req->iNPC_ID || ItemManager::VendorTables.find(req->iNPC_ID) == ItemManager::VendorTables.end())
        return;

    std::vector<VendorListing> listings = ItemManager::VendorTables[req->iNPC_ID]; // maybe use iVendorID instead...?

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC, resp);

    for (int i = 0; i < listings.size() && i < 20; i++) { // 20 is the max
        sItemBase base;
        base.iID = listings[i].iID;
        base.iOpt = 0;
        base.iTimeLimit = 0;
        base.iType = listings[i].type;

        sItemVendor vItem;
        vItem.item = base;
        vItem.iSortNum = listings[i].sort;
        vItem.iVendorID = req->iVendorID;
        //vItem.fBuyCost = listings[i].price; this value is not actually the one that is used

        resp.item[i] = vItem;
    }

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

void NPCManager::npcVendorBuyBattery(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY* req = (sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int cost = req->Item.iOpt * 100;
    if ((req->Item.iID == 3 ? (plr->batteryW >= 9999) : (plr->batteryN >= 9999)) || plr->money < cost) { // sanity check
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL));
    }

    plr->money -= cost;
    plr->batteryW += req->Item.iID == 3 ? req->Item.iOpt * 10 : 0;
    plr->batteryN += req->Item.iID == 4 ? req->Item.iOpt * 10 : 0;

    // caps
    if (plr->batteryW > 9999)
        plr->batteryW = 9999;
    if (plr->batteryN > 9999)
        plr->batteryN = 9999;

    INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC, resp);

    resp.iCandy = plr->money;
    resp.iBatteryW = plr->batteryW;
    resp.iBatteryN = plr->batteryN;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC, sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC));
}

void NPCManager::npcCombineItems(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_COMBINATION))
        return; // malformed packet

    sP_CL2FE_REQ_PC_ITEM_COMBINATION* req = (sP_CL2FE_REQ_PC_ITEM_COMBINATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    
    if (req->iCostumeItemSlot < 0 || req->iCostumeItemSlot >= AINVEN_COUNT || req->iStatItemSlot < 0 || req->iStatItemSlot >= AINVEN_COUNT) { // sanity check 1
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, failResp);
        failResp.iCostumeItemSlot = req->iCostumeItemSlot;
        failResp.iStatItemSlot = req->iStatItemSlot;
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL));
        std::cout << "[WARN] Inventory slot(s) out of range (" << req->iStatItemSlot << " and " << req->iCostumeItemSlot << ")" << std::endl;
        return;
    }

    sItemBase* itemStats = &plr->Inven[req->iStatItemSlot];
    sItemBase* itemLooks = &plr->Inven[req->iCostumeItemSlot];
    Item* itemStatsDat = ItemManager::getItemData(itemStats->iID, itemStats->iType);
    Item* itemLooksDat = ItemManager::getItemData(itemLooks->iID, itemLooks->iType);

    if (itemStatsDat == nullptr || itemLooksDat == nullptr
        || ItemManager::CrocPotTable.find(abs(itemStatsDat->level - itemLooksDat->level)) == ItemManager::CrocPotTable.end()) // sanity check 2
    {
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, failResp);
        failResp.iCostumeItemSlot = req->iCostumeItemSlot;
        failResp.iStatItemSlot = req->iStatItemSlot;
        failResp.iErrorCode = 0;
        std::cout << "[WARN] Either item ids or croc pot value set not found" << std::endl;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL));
        return;
    }

    CrocPotEntry* recipe = &ItemManager::CrocPotTable[abs(itemStatsDat->level - itemLooksDat->level)];
    int cost = itemStatsDat->buyPrice * recipe->multStats + itemLooksDat->buyPrice * recipe->multLooks;
    float successChance = recipe->base / 100.0f; // base success chance

    // rarity gap multiplier
    switch(abs(itemStatsDat->rarity - itemLooksDat->rarity))
    {
    case 0:
        successChance *= recipe->rd0;
        break;
    case 1:
        successChance *= recipe->rd1;
        break;
    case 2:
        successChance *= recipe->rd2;
        break;
    case 3:
        successChance *= recipe->rd3;
        break;
    default:
        break;
    }

    float rolled = (rand() * 1.0f / RAND_MAX) * 100.0f; // success chance out of 100
    //std::cout << rolled << " vs " << successChance << std::endl;
    plr->money -= cost;


    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC, resp);
    if (rolled < successChance) {
        // success
        resp.iSuccessFlag = 1;

        // modify the looks item with the new stats and set the appearance through iOpt
        itemLooks->iOpt = (int32_t)itemLooks->iID << 16;
        itemLooks->iID = itemStats->iID;

        // delete stats item
        itemStats->iID = 0;
        itemStats->iOpt = 0;
        itemStats->iTimeLimit = 0;
        itemStats->iType = 0;
    }
    else {
        // failure; don't do anything?
        resp.iSuccessFlag = 0;
    }
    resp.iCandy = plr->money;
    resp.iNewItemSlot = req->iCostumeItemSlot;
    resp.iStatItemSlot = req->iStatItemSlot;
    resp.sNewItem = *itemLooks;
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_COMBINATION_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC));
}

void NPCManager::npcBarkHandler(CNSocket* sock, CNPacketData* data) {} // stubbed for now

void NPCManager::npcUnsummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_UNSUMMON))
        return; // malformed packet

    if (PlayerManager::getPlayer(sock)->accountLevel > 30)
        return;
    
    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::removeNPC(req->iNPC_ID);
}

void NPCManager::npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    INITSTRUCT(sP_FE2CL_NPC_ENTER, resp);
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    if (plr->accountLevel > 30 || req->iNPCType >= 3314)
        return;

    resp.NPCAppearanceData.iNPC_ID = NPCs.size()+1;
    resp.NPCAppearanceData.iNPCType = req->iNPCType;
    resp.NPCAppearanceData.iHP = 1000; // TODO: placeholder
    resp.NPCAppearanceData.iX = plr->x;
    resp.NPCAppearanceData.iY = plr->y;
    resp.NPCAppearanceData.iZ = plr->z;

    NPCs[resp.NPCAppearanceData.iNPC_ID] = new BaseNPC(plr->x, plr->y, plr->z, req->iNPCType);
    NPCs[resp.NPCAppearanceData.iNPC_ID]->appearanceData.iNPC_ID = resp.NPCAppearanceData.iNPC_ID;

    ChunkManager::addNPC(plr->x, plr->y, resp.NPCAppearanceData.iNPC_ID);
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
    PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
    plrv.currentChunks.clear();
    plrv.chunkPos = std::make_pair<int, int>(0, 0);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
}
