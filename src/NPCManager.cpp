#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "ChunkManager.hpp"
#include "NanoManager.hpp"
#include "TableData.hpp"
#include "ChatManager.hpp"

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
/// sock, CBFlag -> until
std::map<std::pair<CNSocket*, int32_t>, time_t> NPCManager::EggBuffs;
std::unordered_map<int, EggType> NPCManager::EggTypes;
std::unordered_map<int, Egg*> NPCManager::Eggs;
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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SHINY_PICKUP, eggPickup);

    REGISTER_SHARD_TIMER(eggStep, 1000);
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
    case NPC_EGG:
        INITSTRUCT(sP_FE2CL_SHINY_EXIT, exitEggData);
        exitEggData.iShinyID = id;

        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitEggData, P_FE2CL_SHINY_EXIT, sizeof(sP_FE2CL_SHINY_EXIT));
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
    case NPC_EGG:
        INITSTRUCT(sP_FE2CL_SHINY_ENTER, enterEggData);
        npcDataToEggData(&npc->appearanceData, &enterEggData.ShinyAppearanceData);

        for (Chunk* chunk : viewableChunks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterEggData, P_FE2CL_SHINY_ENTER, sizeof(sP_FE2CL_SHINY_ENTER));
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

    // remove from eggs
    if (Eggs.find(id) != Eggs.end())
        Eggs.erase(id);

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

    ChunkManager::addNPC(X, Y, npc->instanceID, id);
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

int NPCManager::eggBuffPlayer(CNSocket* sock, int skillId, int duration) {

    Player* plr = PlayerManager::getPlayer(sock);

    int32_t CBFlag = -1, iValue = 0, CSTB, EST;

    // damage and heal have to be set by hand
    if (skillId == 183) {
        // damage
        CBFlag = CSB_BIT_INFECTION;
        CSTB = ECSB_INFECTION;
        EST = EST_INFECTIONDAMAGE;
    }
    else if (skillId == 150) {
        // heal
        CBFlag = CSB_BIT_HEAL;
        CSTB = ECSB_HEAL;
        EST = EST_HEAL_HP;
    }
    else {
        // find the right passive power data  
        for (auto& pwr : NanoManager::PassivePowers)
            if (pwr.powers.count(skillId)) {
                CBFlag = pwr.iCBFlag;
                CSTB = pwr.eCharStatusTimeBuffID;
                EST = pwr.eSkillType;
                iValue = pwr.iValue;
            }
    }

    if (CBFlag < 0)
        return -1;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, CBFlag);

    // if player doesn't have this buff yet
    if (EggBuffs.find(key) == EggBuffs.end())
    {
        // save new cbflag serverside
        plr->iEggConditionBitFlag |= CBFlag;

        // send buff update package
        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, updatePacket);
        updatePacket.eCSTB = CSTB; // eCharStatusTimeBuffID
        updatePacket.eTBU = 1; // eTimeBuffUpdate 1 means Add
        updatePacket.eTBT = 3; // eTimeBuffType 3 means egg
        updatePacket.TimeBuff.iValue = iValue;
        int32_t updatedFlag = plr->iConditionBitFlag | plr->iGroupConditionBitFlag | plr->iEggConditionBitFlag;
        updatePacket.iConditionBitFlag = updatedFlag;

        sock->sendPacket((void*)&updatePacket, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    // save the buff serverside;
    // if you get the same buff again, new duration will override the previous one
    time_t until = getTime() + (time_t)duration * 1000;
    EggBuffs[key] = until;
    
    /*
     * to give player a visual effect (eg. blue particles for run or wings for jump)
     * we have to send him NANO_SKILL_USE packet with nano Id set to 0
     * yes, this is utterly stupid and disgusting
     */

    const size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Buff);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_NANO_SKILL_USE* skillUse = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Buff* skill = (sSkillResult_Buff*)(respbuf + sizeof(sP_FE2CL_NANO_SKILL_USE));

    memset(respbuf, 0, resplen);

    skillUse->iPC_ID = plr->iID;
    skillUse->iSkillID = skillId;
    skillUse->iNanoID = plr->activeNano;
    skillUse->iNanoStamina = plr->activeNano < 1 ? 0 : plr->Nanos[plr->activeNano].iStamina;
    skillUse->eST = EST;
    skillUse->iTargetCnt = 1;

    skill->eCT = 1;
    skill->iID = plr->iID;
    skill->iConditionBitFlag = plr->iConditionBitFlag | plr->iGroupConditionBitFlag | plr->iEggConditionBitFlag;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);

    return 0;
}

void NPCManager::eggStep(CNServer* serv, time_t currTime) {
    // tick buffs
    time_t timeStamp = currTime;
    auto it = EggBuffs.begin();
    while (it != EggBuffs.end()) {
        // check remaining time
        if (it->second > timeStamp)
            it++;

        // if time reached 0
        else {
            CNSocket* sock = it->first.first;
            Player* plr = PlayerManager::getPlayer(sock);
           
            // if player is still on the server
            if (plr != nullptr) {

                int32_t CBFlag = it->first.second;
                int32_t CSTB = -1, iValue = 0;

                // find CSTB Value
                if (CBFlag == CSB_BIT_INFECTION)
                    CSTB = ECSB_INFECTION;           

                else if (CBFlag == CSB_BIT_HEAL)
                    CSTB = ECSB_HEAL;

                else {
                    for (auto pwr : NanoManager::PassivePowers) {
                        if (pwr.iCBFlag == CBFlag) {
                            CSTB = pwr.eCharStatusTimeBuffID;
                            iValue = pwr.iValue;
                            break;
                        }
                    }
                }

                if (CSTB > 0) {
                    // update CBFlag serverside
                    plr->iEggConditionBitFlag &= ~CBFlag;
                    // send buff update packet
                    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, updatePacket);
                    updatePacket.eCSTB = CSTB; // eCharStatusTimeBuffID
                    updatePacket.eTBU = 2; // eTimeBuffUpdate 2 means remove
                    updatePacket.eTBT = 3; // eTimeBuffType 3 means egg
                    updatePacket.iConditionBitFlag = plr->iConditionBitFlag | plr->iGroupConditionBitFlag | plr->iEggConditionBitFlag;
                    updatePacket.TimeBuff.iValue = iValue;
                    sock->sendPacket((void*)&updatePacket, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
                }
            }
            // remove buff from the map
            it = EggBuffs.erase(it);
        }
    }

    // check dead eggs and eggs in inactive chunks
    for (auto egg : Eggs) {
        if (!egg.second->dead || !ChunkManager::inPopulatedChunks(egg.second->appearanceData.iX, egg.second->appearanceData.iY, egg.second->instanceID))
            continue;
        if (egg.second->deadUntil <= timeStamp) {
            // respawn it
            egg.second->dead = false;
            egg.second->deadUntil = 0;
            egg.second->appearanceData.iHP = 400;
            addNPC(egg.second->currentChunks, egg.first);
        }
    }

}

void NPCManager::npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg) {
    egg->iX = npc->iX;
    egg->iY = npc->iY;
    egg->iZ = npc->iZ;
    // client doesn't care about egg->iMapNum
    egg->iShinyType = npc->iNPCType;
    egg->iShiny_ID = npc->iNPC_ID;
}

void NPCManager::eggPickup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SHINY_PICKUP))
        return; // malformed packet

    sP_CL2FE_REQ_SHINY_PICKUP* pickup = (sP_CL2FE_REQ_SHINY_PICKUP*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int eggId = pickup->iShinyID;

    if (Eggs.find(eggId) == Eggs.end()) {
        std::cout << "[WARN] Player tried to open non existing egg?!" << std::endl;
        return;
    }
    Egg* egg = Eggs[eggId];

    if (egg->dead) {
        std::cout << "[WARN] Player tried to open a dead egg?!" << std::endl;
        return;
    }

    if (abs(egg->appearanceData.iX - plr->x)>500 || abs(egg->appearanceData.iY - plr->y) > 500) {
        std::cout << "[WARN] Player tried to open an egg from the other chunk?!" << std::endl;
        return;
    }

    int typeId = egg->appearanceData.iNPCType;
    if (EggTypes.find(typeId) == EggTypes.end()) {
        if (egg->npcClass != NPCClass::NPC_EGG) {
            std::cout << "[WARN] Egg Type " << typeId << " not found!" << std::endl;
            return;
        }
    }
    EggType* type = &EggTypes[typeId];

    // buff the player
    if (type->effectId != 0)
        eggBuffPlayer(sock, type->effectId, type->duration);

    // damage egg
    if (type->effectId == 183) {
        size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_Damage);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

        memset(respbuf, 0, resplen);

        sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK* pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
        sSkillResult_Damage* dmg = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

        dmg->iDamage = PC_MAXHEALTH(plr->level) * 6 / 10;

        plr->HP -= dmg->iDamage;

        pkt->iID = plr->iID;
        pkt->eCT = 1; // player
        pkt->iTB_ID = 19;
        dmg->iHP = plr->HP;

        sock->sendPacket((void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
        PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
    }
    // heal egg
    else if (type->effectId == 150) {
        size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_Heal_HP);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

        memset(respbuf, 0, resplen);

        sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK* pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
        sSkillResult_Heal_HP* heal = (sSkillResult_Heal_HP*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

        heal->iHealHP = PC_MAXHEALTH(plr->level) * 35 / 100;
        plr->HP += heal->iHealHP;
        if (plr->HP > PC_MAXHEALTH(plr->level))
            plr->HP = PC_MAXHEALTH(plr->level);

        heal->iHP = plr->HP;
        heal->iID = plr->iID;
        heal->eCT = 1;

        pkt->iID = plr->iID;
        pkt->eCT = 1; // player
        pkt->iTB_ID = ECSB_HEAL;

        sock->sendPacket((void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
        PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
    }

    /*
     * SHINY_PICKUP_SUCC is only causing a GUI effect in the client
     * (buff icon pops up in the bottom of the screen)
     * so we don't send it for non-effect
     */

    if (type->effectId != 0)
    {
        INITSTRUCT(sP_FE2CL_REP_SHINY_PICKUP_SUCC, resp);
        resp.iSkillID = type->effectId;

        // in general client finds correct icon on it's own,
        // but for damage we have to supply correct CSTB
        if (resp.iSkillID == 183)
            resp.eCSTB = ECSB_INFECTION;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_SHINY_PICKUP_SUCC, sizeof(sP_FE2CL_REP_SHINY_PICKUP_SUCC));
    }

    // drop
    if (type->dropCrateId != 0) {
        const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        // we know it's only one trailing struct, so we can skip full validation

        uint8_t respbuf[resplen]; // not a variable length array, don't worry
        sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
        sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

        // don't forget to zero the buffer!
        memset(respbuf, 0, resplen);

        // send back player's stats
        reward->m_iCandy = plr->money;
        reward->m_iFusionMatter = plr->fusionmatter;
        reward->m_iBatteryN = plr->batteryN;
        reward->m_iBatteryW = plr->batteryW;
        reward->iFatigue = 100; // prevents warning message
        reward->iFatigue_Level = 1;
        reward->iItemCnt = 1; // remember to update resplen if you change this

        int slot = ItemManager::findFreeSlot(plr);

        // no space for drop
        if (slot != -1) {

            // item reward
            item->sItem.iType = 9;
            item->sItem.iOpt = 1;
            item->sItem.iID = type->dropCrateId;
            item->iSlotNum = slot;
            item->eIL = 1; // Inventory Location. 1 means player inventory.

            // update player
            plr->Inven[slot] = item->sItem;
            sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
        }
    }

    if (egg->summoned)
        destroyNPC(eggId);
    else {
        removeNPC(egg->currentChunks, eggId);
        egg->dead = true;
        egg->deadUntil = getTime() + (time_t)type->regen * 1000;
        egg->appearanceData.iHP = 0;
    }
}
