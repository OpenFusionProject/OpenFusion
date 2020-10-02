#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>
#include <assert.h>

#include "contrib/JSON.hpp"

std::map<int32_t, BaseNPC*> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;
nlohmann::json NPCManager::NPCData;

/*
 * Initialized at the end of TableData::init().
 * This allows us to summon and kill mobs in arbitrary order without
 * NPC ID collisions.
 */
int32_t NPCManager::nextId;

void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TIME_TO_GO_WARP, npcWarpTimeMachine);
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

void NPCManager::removeNPC(std::vector<Chunk*> viewableChunks, int32_t id) {
    BaseNPC* npc = NPCs[id];

    switch (npc->npcClass) {
    case NPC_BUS:
        INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, exitBusData);
        exitBusData.eTT = 3;
        exitBusData.iT_ID = id;

        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitBusData, P_FE2CL_TRANSPORTATION_EXIT, sizeof(sP_FE2CL_TRANSPORTATION_EXIT));
            }
        }
        break;
    default:
        // create struct
        INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
        exitData.iNPC_ID = id;

        // remove it from the clients
        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
            }
        }
        break;
    }
}

void NPCManager::addNPC(std::vector<Chunk*> viewableChunks, int32_t id) {
    BaseNPC* npc = NPCs[id];

    switch (npc->npcClass) {
    case NPC_BUS:
        INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, enterBusData);
        enterBusData.AppearanceData = { 3, npc->appearanceData.iNPC_ID, npc->appearanceData.iNPCType, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ };

        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterBusData, P_FE2CL_TRANSPORTATION_ENTER, sizeof(sP_FE2CL_TRANSPORTATION_ENTER));
            }
        }
        break;
    default:
        // create struct
        INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
        enterData.NPCAppearanceData = npc->appearanceData;

        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
            }
        }
        break;
    }
}

void NPCManager::destroyNPC(int32_t id) {
    // sanity check
    if (NPCs.find(id) == NPCs.end()) {
        std::cout << "npc not found : " << id << std::endl;
        return;
    }

    BaseNPC* entity = NPCs[id];

    // sanity check
    if (ChunkManager::chunks.find(entity->chunkPos) == ChunkManager::chunks.end()) {
        std::cout << "chunk not found!" << std::endl;
        return;
    }

    // remove NPC from the chunk
    Chunk* chunk = ChunkManager::chunks[entity->chunkPos];
    chunk->NPCs.erase(id);
    
    // remove from viewable chunks
    removeNPC(entity->currentChunks, id);

    // remove from mob manager
    if (MobManager::Mobs.find(id) != MobManager::Mobs.end())
            MobManager::Mobs.erase(id);
    
    // finally, remove it from the map and free it
    NPCs.erase(id);
    delete entity;

    std::cout << "npc removed!" << std::endl;
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z) {
    BaseNPC* npc = NPCs[id];

    npc->appearanceData.iX = X;
    npc->appearanceData.iY = Y;
    npc->appearanceData.iZ = Z;
    std::tuple<int, int, int> newPos = ChunkManager::grabChunk(X, Y,npc->imapNum);

    // nothing to be done (but we should also update currentChunks to add/remove stale chunks)
    if (newPos == npc->chunkPos) {
        npc->currentChunks = ChunkManager::grabChunks(newPos);
        return;
    }

    std::vector<Chunk*> allChunks = ChunkManager::grabChunks(newPos);

    // send npc exit to stale chunks
    removeNPC(ChunkManager::getDeltaChunks(npc->currentChunks, allChunks), id);

    // send npc enter to new chunks
    addNPC(ChunkManager::getDeltaChunks(allChunks, npc->currentChunks), id);

    // update chunks
    ChunkManager::removeNPC(npc->chunkPos, id);
    ChunkManager::addNPC(X, Y, npc->imapNum, id);

    npc->chunkPos = newPos;
    npc->currentChunks = allChunks;
}

void NPCManager::sendToViewable(BaseNPC *npc, void *buf, uint32_t type, size_t size) {
    for (Chunk *chunk : npc->currentChunks) {
        for (CNSocket *s : chunk->players) {
            s->sendPacket(buf, type, size);
        }
    }
}

void NPCManager::npcVendorBuy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY* req = (sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

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
    // if vehicle
    if (req->Item.iType == 10)
        // set time limit: current time + 7days
        req->Item.iTimeLimit = getTimestamp() + 604800;

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

    if (plr == nullptr)
        return;

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

    if (plr == nullptr)
        return;

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

    if (plr == nullptr)
        return;

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

    if (plr == nullptr)
        return;
    
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

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr || plr->accountLevel > 30)
        return;
    
    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::destroyNPC(req->iNPC_ID);
}

void NPCManager::npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    INITSTRUCT(sP_FE2CL_NPC_ENTER, resp);
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    if (plr == nullptr || plr->accountLevel > 30 || req->iNPCType >= 3314)
        return;

    int team = NPCData[req->iNPCType]["m_iTeam"];

    assert(nextId < INT32_MAX);
    resp.NPCAppearanceData.iNPC_ID = nextId++;
    resp.NPCAppearanceData.iNPCType = req->iNPCType;
    resp.NPCAppearanceData.iHP = 1000;
    resp.NPCAppearanceData.iX = plr->x;
    resp.NPCAppearanceData.iY = plr->y;
    resp.NPCAppearanceData.iZ = plr->z;

    if (team == 2) {
        NPCs[resp.NPCAppearanceData.iNPC_ID] = new Mob(plr->x, plr->y, plr->z, plr->mapNum, req->iNPCType, NPCData[req->iNPCType], resp.NPCAppearanceData.iNPC_ID);
        MobManager::Mobs[resp.NPCAppearanceData.iNPC_ID] = (Mob*)NPCs[resp.NPCAppearanceData.iNPC_ID];
    } else
        NPCs[resp.NPCAppearanceData.iNPC_ID] = new BaseNPC(plr->x, plr->y, plr->z, req->iNPCType, resp.NPCAppearanceData.iNPC_ID, plr->mapNum);

    updateNPCPosition(resp.NPCAppearanceData.iNPC_ID, plr->x, plr->y, plr->z);
}

void NPCManager::npcWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_NPC* warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;
    handleWarp(sock, warpNpc->iWarpID);
}

void NPCManager::npcWarpTimeMachine(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TIME_TO_GO_WARP))
        return; // malformed packet
    // this is just a warp request
    handleWarp(sock, 28);
}

void NPCManager::handleWarp(CNSocket* sock, int32_t warpId) {
    PlayerView& plrv = PlayerManager::players[sock];
    // sanity check
    if (Warps.find(warpId) == Warps.end())
        return;

    // send to client
    INITSTRUCT(sP_FE2CL_INSTANCE_MAP_INFO, instanceInfo);

    std::cerr << "Warped to Map Num:" << Warps[warpId].mapNum << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance)
    {
        INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, response);
        response.iX = Warps[warpId].x;
        response.iY = Warps[warpId].y;
        response.iZ = Warps[warpId].z;
        instanceInfo.iInstanceMapNum = Warps[warpId].mapNum;
        plrv.plr->mapNum = Warps[warpId].mapNum;
        PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
        plrv.currentChunks.clear();
        sock->sendPacket((void*)&instanceInfo, P_FE2CL_INSTANCE_MAP_INFO, sizeof(sP_FE2CL_INSTANCE_MAP_INFO));
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); //Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpId].x;
        resp.iY = Warps[warpId].y;
        resp.iZ = Warps[warpId].z;
        PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
        plrv.currentChunks.clear();
        plrv.plr->mapNum = 0;
        plrv.plr->mapUID = 0;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
    }
}
void NPCManager::changeNPCMAP(CNSocket* sock, PlayerView& view, int mapNum) {
    std::cout << "Change NPC Map" << std::endl;
    std::ifstream inFile(settings::NPCJSON);
    nlohmann::json npcData;
    nlohmann::json editData;
    std::ifstream editInFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
    if (editInFile.fail())
    {
        std::cout << "file does not exist" << std::endl;
    }
    else
    {
        editInFile >> editData;
        editInFile.close();
    }
    std::map<int32_t, BaseNPC*> currentNpcs;
    // read file into json
    inFile >> npcData;
    inFile.close();
    currentNpcs = NPCs;
    if (view.plr->mapUID != 0)
    {
        //currentNpcs = PrivateInstances[view.plr->mapUID].NPCs;
    }
    for (auto& pair : currentNpcs) {
        if (view.plr->mapNum == pair.second->imapNum && pair.second->appearanceData.iNPCType == npcData[std::to_string(pair.second->jsonRef)]["id"])
        {
            //std::cout << "aaa"  << std::endl;
            int diffX = abs(view.plr->x - pair.second->appearanceData.iX);
            int diffY = abs(view.plr->y - pair.second->appearanceData.iY);

            if (diffX < 100 && diffY < 100)
            {
                std::cout << "npcId" << pair.second->appearanceData.iNPCType << " ChangeNpcMap Map ID: " << mapNum << "NPCID: " << pair.second->jsonRef << "Name" << npcData[std::to_string(pair.second->jsonRef)]["name"] << std::endl;
                npcData[std::to_string(pair.second->jsonRef)]["mapNum"] = (mapNum);
                std::cout << npcData[std::to_string(pair.second->jsonRef)]["mapNum"] << std::endl;
                std::cout << npcData[std::to_string(pair.second->jsonRef)] << std::endl;
                //remove((settings::NPCJSON).data());
                editData[std::to_string(pair.second->jsonRef)] = npcData[std::to_string(pair.second->jsonRef)];
                std::ofstream outFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
                outFile << editData << std::endl;
                outFile.close();
                return;
            }

        }

    }
}
void NPCManager::reloadNPCs()
{
    // load NPCs from NPCs.json into our NPC manager
    //If someone knows how to reset all chunks please do that here
    int i = 0;
    NPCs.clear();
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            BaseNPC* tmp = new BaseNPC(npc["x"], npc["y"], npc["z"], npc["id"], npc["mapNum"], std::stoi(_npc.key()));

            // Temporary fix, IDs will be pulled from json later
            tmp->appearanceData.iNPC_ID = i;

            NPCManager::NPCs[i] = tmp;
            ChunkManager::addNPC(npc["x"], npc["y"], npc["mapNum"], i);

            i++;

            if (npc["id"] == 641 || npc["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["x"], npc["y"], ((int)npc["z"]) + RESURRECT_HEIGHT,(int)npc["mapNum"] });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }
    // load everything else from xdttable
    std::cout << "[INFO] Parsing xdt.json..." << std::endl;
    std::ifstream infile(settings::XDTJSON);
    nlohmann::json xdtData;

    // read file into json
    infile >> xdtData;
    // load temporary mob dump
    try {
        std::ifstream inFile(settings::MOBJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        nlohmann::json npcTableData = xdtData["m_pNpcTable"]["m_pNpcData"];

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            auto td = npcTableData[(int)npc["iNPCType"]];
            Mob* tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iNPCType"], npc["iHP"], npc["iAngle"], td["m_iRegenTime"], npc["mapNum"], std::stoi(_npc.key()));

            // Temporary fix, IDs will be pulled from json later
            tmp->appearanceData.iNPC_ID = i;

            NPCManager::NPCs[i] = tmp;
            MobManager::Mobs[i] = (Mob*)NPCManager::NPCs[i];
            ChunkManager::addNPC(npc["iX"], npc["iY"], npc["mapNum"], i);


            i++;
        }

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
    }
    ChunkManager::chunks.clear();

}
void NPCManager::SummonWrite(CNSocket* sock, PlayerView& view, int NPCID)
{
    //Unfinished Ignore
    std::cout << "SummonWrite" << std::endl;

    INITSTRUCT(sP_FE2CL_NPC_ENTER, resp);
    Player* plr = PlayerManager::getPlayer(sock);

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
    nlohmann::json editData;
    std::ifstream editInFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
    if (editInFile.fail())
    {
        std::cout << "file does not exist" << std::endl;
    }
    else
    {
        editInFile >> editData;
        editInFile.close();
    }
    std::string validId;
    int i = 0;
    for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++, i++) {
        if (npcData[std::to_string(i)]["name"] == "" && editData[std::to_string(i)]["name"] != "Temp")
        {
            validId = std::to_string(i);
            npcData[validId]["id"] = NPCID;
            npcData[validId]["mapNum"] = plr->mapNum;
            npcData[validId]["name"] = "Temp";
            npcData[validId]["x"] = resp.NPCAppearanceData.iX;
            npcData[validId]["y"] = resp.NPCAppearanceData.iY;
            npcData[validId]["z"] = resp.NPCAppearanceData.iZ;
            std::cout << npcData[validId] << std::endl;

            editData[validId] = npcData[validId];
            std::ofstream outFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
            outFile << editData << std::endl;
            outFile.close();
            std::cout << "SendPacket" << std::endl;
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
    nlohmann::json editData;
    std::ifstream editInFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
    if (editInFile.fail())
    {
        std::cout << "file does not exist" << std::endl;
    }
    else
    {
        editInFile >> editData;
        editInFile.close();
    }
    for (auto& pair : NPCs) {
        if (view.plr->mapNum == pair.second->imapNum && pair.second->appearanceData.iNPCType == npcData[std::to_string(pair.second->jsonRef)]["id"])
        {
            int diffX = abs(view.plr->x - pair.second->appearanceData.iX);
            int diffY = abs(view.plr->y - pair.second->appearanceData.iY);

            if (diffX < 100 && diffY < 100)
            {
                std::cout << std::to_string(pair.second->jsonRef) << npcData[std::to_string(pair.second->jsonRef)] << std::endl;
                npcData[std::to_string(pair.second->jsonRef)]["id"] = 0;
                npcData[std::to_string(pair.second->jsonRef)]["mapNum"] = 0;
                npcData[std::to_string(pair.second->jsonRef)]["name"] = "";
                npcData[std::to_string(pair.second->jsonRef)]["x"] = 0;
                npcData[std::to_string(pair.second->jsonRef)]["y"] = 0;
                npcData[std::to_string(pair.second->jsonRef)]["z"] = 0;
                editData[std::to_string(pair.second->jsonRef)] = npcData[std::to_string(pair.second->jsonRef)];
                std::ofstream outFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
                outFile << editData << std::endl;
                outFile.close();
                return;
            }

        }

    }


}
void NPCManager::tableWrite(CNSocket* sock, PlayerView& view, int PCID)
{
    std::ifstream mainFile(settings::NPCJSON);
    nlohmann::json npcData;

    nlohmann::json editData;
    mainFile >> editData;
    std::ifstream inFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string((PCID)) + ".json");
    if (inFile.fail())
    {
        std::cout << settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) << std::to_string((PCID)) << ".json did not exist" << std::endl;
        return;
    }
    inFile >> npcData;
    for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
        editData[npc.key()] = npcData[npc.key()];
    }
    std::ofstream outFile(settings::NPCJSON);
    outFile << editData << std::endl;
    outFile.close();
    clearChanges(sock, PlayerManager::players[sock], PCID);
    reloadNPCs();
    return;
}
void NPCManager::clearChanges(CNSocket* sock, PlayerView& view)
{
    nlohmann::json editData;
    std::ofstream outFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(view.plr->iID) + ".json");
    outFile << editData << std::endl;
    outFile.close();

    return;
}
void NPCManager::clearChanges(CNSocket* sock, PlayerView& view, int playerUID)
{
    nlohmann::json editData;
    std::ofstream outFile(settings::NPCJSON.substr(0, settings::NPCJSON.find_last_of(".")) + std::to_string(playerUID) + ".json");
    outFile << editData << std::endl;
    outFile.close();

    return;
}
