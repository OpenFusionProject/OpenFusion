#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "ChunkManager.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>
#include <assert.h>
#include <limits.h>

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
        std::cout << "npc not found: " << id << std::endl;
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
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z, int angle) {
    NPCs[id]->appearanceData.iAngle = angle;
    updateNPCPosition(id, X, Y, Z);
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z) {
    BaseNPC* npc = NPCs[id];

    npc->appearanceData.iX = X;
    npc->appearanceData.iY = Y;
    npc->appearanceData.iZ = Z;
    std::tuple<int, int, uint64_t> newPos = ChunkManager::grabChunk(X, Y, npc->instanceID);

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

    Chunk *chunk = nullptr;
    if (ChunkManager::checkChunk(npc->chunkPos))
        chunk = ChunkManager::chunks[npc->chunkPos];

    if (ChunkManager::removeNPC(npc->chunkPos, id)) {
        // if the old chunk was deallocated, remove it
        allChunks.erase(std::remove(allChunks.begin(), allChunks.end(), chunk), allChunks.end());
    }

    ChunkManager::addNPC(X, Y, npc->instanceID, id);

    npc->chunkPos = newPos;
    npc->currentChunks = allChunks;
}

void NPCManager::updateNPCInstance(int32_t npcID, uint64_t instanceID) {
    BaseNPC* npc = NPCs[npcID];
    npc->instanceID = instanceID;
    updateNPCPosition(npcID, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ);
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

    int itemCost = item->buyPrice * (item->stackSize > 1 ? req->Item.iOpt : 1);
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
    } else { // selling entire slot
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

    for (int i = 0; i < (int)listings.size() && i < 20; i++) { // 20 is the max
        sItemBase base;
        base.iID = listings[i].iID;
        base.iOpt = 0;
        base.iTimeLimit = 0;
        base.iType = listings[i].type;

        sItemVendor vItem;
        vItem.item = base;
        vItem.iSortNum = listings[i].sort;
        vItem.iVendorID = req->iVendorID;
        //vItem.fBuyCost = listings[i].price; // this value is not actually the one that is used

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

    cost = plr->batteryW + plr->batteryN;
    plr->batteryW += req->Item.iID == 3 ? req->Item.iOpt * 100 : 0;
    plr->batteryN += req->Item.iID == 4 ? req->Item.iOpt * 100 : 0;

    // caps
    if (plr->batteryW > 9999)
        plr->batteryW = 9999;
    if (plr->batteryN > 9999)
        plr->batteryN = 9999;

    cost = plr->batteryW + plr->batteryN - cost;
    plr->money -= cost;

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
        || ItemManager::CrocPotTable.find(abs(itemStatsDat->level - itemLooksDat->level)) == ItemManager::CrocPotTable.end()) { // sanity check 2
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
    switch(abs(itemStatsDat->rarity - itemLooksDat->rarity)) {
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
        itemLooks->iOpt = (int32_t)((itemLooks->iOpt) >> 16 > 0 ? (itemLooks->iOpt >> 16) : itemLooks->iID) << 16;
        itemLooks->iID = itemStats->iID;

        // delete stats item
        itemStats->iID = 0;
        itemStats->iOpt = 0;
        itemStats->iTimeLimit = 0;
        itemStats->iType = 0;
    } else {
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
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    if (plr == nullptr || plr->accountLevel > 30 || req->iNPCType >= 3314 || req->iNPCCnt > 100)
        return;

    int team = NPCData[req->iNPCType]["m_iTeam"];

    for (int i = 0; i < req->iNPCCnt; i++) {
        assert(nextId < INT32_MAX);
        int id = nextId++;

        if (team == 2) {
            NPCs[id] = new Mob(plr->x, plr->y, plr->z, plr->instanceID, req->iNPCType, NPCData[req->iNPCType], id);
            MobManager::Mobs[id] = (Mob*)NPCs[id];
        } else
            NPCs[id] = new BaseNPC(plr->x, plr->y, plr->z, 0, plr->instanceID, req->iNPCType, id);

        updateNPCPosition(id, plr->x, plr->y, plr->z);
    }
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

    // std::cerr << "Warped to Map Num:" << Warps[warpId].instanceID << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance) {
        uint64_t instanceID = Warps[warpId].instanceID;

        // if warp requires you to be on a mission, it's gotta be a unique instance
        if (Warps[warpId].limitTaskID != 0 || instanceID == 14) { // 14 is a special case for the Time Lab
            instanceID += ((uint64_t)plrv.plr->iIDGroup << 32); // upper 32 bits are leader ID
            ChunkManager::createInstance(instanceID);
        }

        if (plrv.plr->iID == plrv.plr->iIDGroup && plrv.plr->groupCnt == 1)
            PlayerManager::sendPlayerTo(sock, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
        else {
            Player* leaderPlr = PlayerManager::getPlayerFromID(plrv.plr->iIDGroup);

            for (int i = 0; i < leaderPlr->groupCnt; i++) {
                Player* otherPlr = PlayerManager::getPlayerFromID(leaderPlr->groupIDs[i]);
                CNSocket* sockTo = PlayerManager::getSockFromID(leaderPlr->groupIDs[i]);

                if (otherPlr == nullptr || sockTo == nullptr)
                    continue;

                PlayerManager::sendPlayerTo(sockTo, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
            }
        }
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); //Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpId].x;
        resp.iY = Warps[warpId].y;
        resp.iZ = Warps[warpId].z;
        resp.iCandy = plrv.plr->money;
        resp.eIL = 4; // do not take away any items
        PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
        plrv.currentChunks.clear();
        plrv.plr->instanceID = INSTANCE_OVERWORLD;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
    }
}

/*
 * Helper function to get NPC closest to coordinates in specified chunks
 */
BaseNPC* NPCManager::getNearestNPC(std::vector<Chunk*> chunks, int X, int Y, int Z) {
    BaseNPC* npc = nullptr;
    int lastDist = INT_MAX;
    for (auto c = chunks.begin(); c != chunks.end(); c++) { // haha get it
        Chunk* chunk = *c;
        for (auto _npc = chunk->NPCs.begin(); _npc != chunk->NPCs.end(); _npc++) {
            BaseNPC* npcTemp = NPCs[*_npc];
            int distXY = std::hypot(X - npcTemp->appearanceData.iX, Y - npcTemp->appearanceData.iY);
            int dist = std::hypot(distXY, Z - npcTemp->appearanceData.iZ);
            if (dist < lastDist) {
                npc = npcTemp;
                lastDist = dist;
            }
        }
    }
    return npc;
}
