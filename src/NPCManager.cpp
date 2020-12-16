#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "ChunkManager.hpp"
#include "NanoManager.hpp"
#include "TableData.hpp"
#include "ChatManager.hpp"
#include "GroupManager.hpp"

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

void NPCManager::destroyNPC(int32_t id) {
    // sanity check
    if (NPCs.find(id) == NPCs.end()) {
        std::cout << "npc not found: " << id << std::endl;
        return;
    }

    BaseNPC* entity = NPCs[id];

    // sanity check
    if (!ChunkManager::chunkExists(entity->chunkPos)) {
        std::cout << "chunk not found!" << std::endl;
        return;
    }

    // remove NPC from the chunk
    ChunkManager::untrackNPC(entity->chunkPos, id);

    // remove from viewable chunks
    ChunkManager::removeNPCFromChunks(ChunkManager::getViewableChunks(entity->chunkPos), id);

    // remove from mob manager
    if (MobManager::Mobs.find(id) != MobManager::Mobs.end())
        MobManager::Mobs.erase(id);

    // remove from eggs
    if (Eggs.find(id) != Eggs.end())
        Eggs.erase(id);

    // finally, remove it from the map and free it
    delete entity->viewableChunks;
    NPCs.erase(id);
    delete entity;
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z, uint64_t I, int angle) {
    BaseNPC* npc = NPCs[id];
    npc->appearanceData.iAngle = angle;
    ChunkPos oldChunk = npc->chunkPos;
    ChunkPos newChunk = ChunkManager::chunkPosAt(X, Y, I);
    npc->appearanceData.iX = X;
    npc->appearanceData.iY = Y;
    npc->appearanceData.iZ = Z;
    npc->instanceID = I;
    if (oldChunk == newChunk)
        return; // didn't change chunks
    ChunkManager::updateNPCChunk(id, oldChunk, newChunk);
}

void NPCManager::sendToViewable(BaseNPC *npc, void *buf, uint32_t type, size_t size) {
    for (auto it = npc->viewableChunks->begin(); it != npc->viewableChunks->end(); it++) {
        Chunk* chunk = *it;
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

    ItemManager::Item* item = ItemManager::getItemData(req->Item.iID, req->Item.iType);

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

    if (req->iInvenSlotNum < 0 || req->iInvenSlotNum >= AINVEN_COUNT || req->iItemCnt < 0) {
        std::cout << "[WARN] Client failed to sell item in slot " << req->iInvenSlotNum << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, failResp);
        failResp.iErrorCode = 0;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL, sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL));
        return;
    }

    sItemBase* item = &plr->Inven[req->iInvenSlotNum];
    ItemManager::Item* itemData = ItemManager::getItemData(item->iID, item->iType);

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

    ItemManager::Item* item = ItemManager::getItemData(req->Item.iID, req->Item.iType);

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

    if (req->iVendorID != req->iNPC_ID || ItemManager::VendorTables.find(req->iVendorID) == ItemManager::VendorTables.end())
        return;

    std::vector<VendorListing> listings = ItemManager::VendorTables[req->iVendorID];

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
    ItemManager::Item* itemStatsDat = ItemManager::getItemData(itemStats->iID, itemStats->iType);
    ItemManager::Item* itemLooksDat = ItemManager::getItemData(itemLooks->iID, itemLooks->iType);

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

void NPCManager::npcBarkHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_BARKER))
        return; // malformed packet

    sP_CL2FE_REQ_BARKER* req = (sP_CL2FE_REQ_BARKER*)data->buf;

    // get bark IDs from task data
    TaskData* td = MissionManager::Tasks[req->iMissionTaskID];
    std::vector<int> barks;
    for (int i = 0; i < 4; i++) {
        if (td->task["m_iHBarkerTextID"][i] != 0) // non-zeroes only
            barks.push_back(td->task["m_iHBarkerTextID"][i]);
    }

    if (barks.empty())
        return; // no barks

    INITSTRUCT(sP_FE2CL_REP_BARKER, resp);
    resp.iNPC_ID = req->iNPC_ID;
    resp.iMissionStringID = barks[rand() % barks.size()];
    sock->sendPacket((void*)&resp, P_FE2CL_REP_BARKER, sizeof(sP_FE2CL_REP_BARKER));
}

void NPCManager::npcUnsummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_UNSUMMON))
        return; // malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::destroyNPC(req->iNPC_ID);
}

// type must already be checked and updateNPCPosition() must be called on the result
BaseNPC *NPCManager::summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn, bool baseInstance) {
    uint64_t inst = baseInstance ? MAPNUM(instance) : instance;
#define EXTRA_HEIGHT 0

    assert(nextId < INT32_MAX);
    int id = nextId++;
    int team = NPCData[type]["m_iTeam"];
    BaseNPC *npc = nullptr;

    if (team == 2) {
        npc = new Mob(x, y, z + EXTRA_HEIGHT, inst, type, NPCData[type], id);
        MobManager::Mobs[id] = (Mob*)npc;

        // re-enable respawning, if desired
        ((Mob*)npc)->summoned = !respawn;
    } else
        npc = new BaseNPC(x, y, z + EXTRA_HEIGHT, 0, inst, type, id);

    NPCs[id] = npc;

    return npc;
}

void NPCManager::npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // permission & sanity check
    if (plr->accountLevel > 30 || req->iNPCType >= 3314 || req->iNPCCnt > 100)
        return;

    for (int i = 0; i < req->iNPCCnt; i++) {
        BaseNPC *npc = summonNPC(plr->x, plr->y, plr->z, plr->instanceID, req->iNPCType);
        updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, plr->instanceID, 0);
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
    Player* plr = PlayerManager::getPlayer(sock);
    // sanity check
    if (Warps.find(warpId) == Warps.end())
        return;

    // std::cerr << "Warped to Map Num:" << Warps[warpId].instanceID << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance) {
        uint64_t instanceID = Warps[warpId].instanceID;

        // if warp requires you to be on a mission, it's gotta be a unique instance
        if (Warps[warpId].limitTaskID != 0 || instanceID == 14) { // 14 is a special case for the Time Lab
            instanceID += ((uint64_t)plr->iIDGroup << 32); // upper 32 bits are leader ID
            ChunkManager::createInstance(instanceID);

            // save Lair entrance coords as a pseudo-Resurrect 'Em
            plr->recallX = Warps[warpId].x;
            plr->recallY = Warps[warpId].y;
            plr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
            plr->recallInstance = instanceID;
        }

        if (plr->iID == plr->iIDGroup && plr->groupCnt == 1)
            PlayerManager::sendPlayerTo(sock, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
        else {
            Player* leaderPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

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
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); // Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpId].x;
        resp.iY = Warps[warpId].y;
        resp.iZ = Warps[warpId].z;
        resp.iCandy = plr->money;
        resp.eIL = 4; // do not take away any items
        plr->instanceID = INSTANCE_OVERWORLD;
        MissionManager::failInstancedMissions(sock); // fail any instanced missions
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
        ChunkManager::updatePlayerChunk(sock, plr->chunkPos, std::make_tuple(0, 0, 0)); // force player to reload chunks
        PlayerManager::updatePlayerPosition(sock, resp.iX, resp.iY, resp.iZ, INSTANCE_OVERWORLD, plr->angle);
    }
}

/*
 * Helper function to get NPC closest to coordinates in specified chunks
 */
BaseNPC* NPCManager::getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z) {
    BaseNPC* npc = nullptr;
    int lastDist = INT_MAX;
    for (auto c = chunks->begin(); c != chunks->end(); c++) { // haha get it
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

int NPCManager::eggBuffPlayer(CNSocket* sock, int skillId, int eggId) {
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    int bitFlag = GroupManager::getGroupFlags(otherPlr);
    int CBFlag = NanoManager::applyBuff(sock, skillId, 1, 3, bitFlag);

    size_t resplen; 

    if (skillId == 183) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Damage);
    } else if (skillId == 150) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Heal_HP);
    } else {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Buff);
    }
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    sP_FE2CL_NPC_SKILL_HIT* skillUse = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;

    if (skillId == 183) { // damage egg
        sSkillResult_Damage* skill = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iDamage = PC_MAXHEALTH(plr->level) * NanoManager::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP -= skill->iDamage;
        if (plr->HP < 0)
            plr->HP = 0;
        skill->iHP = plr->HP;
    } else if (skillId == 150) { // heal egg
        sSkillResult_Heal_HP* skill = (sSkillResult_Heal_HP*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iHealHP = PC_MAXHEALTH(plr->level) * NanoManager::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP += skill->iHealHP;
        if (plr->HP > PC_MAXHEALTH(plr->level))
            plr->HP = PC_MAXHEALTH(plr->level);
        skill->iHP = plr->HP;
    } else { // regular buff egg
        sSkillResult_Buff* skill = (sSkillResult_Buff*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iConditionBitFlag = plr->iConditionBitFlag;
    }

    skillUse->iNPC_ID = eggId;
    skillUse->iSkillID = skillId;
    skillUse->eST = NanoManager::SkillTable[skillId].skillType;
    skillUse->iTargetCnt = 1;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);

    if (CBFlag == 0)
        return -1;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, CBFlag);

    // save the buff serverside;
    // if you get the same buff again, new duration will override the previous one
    time_t until = getTime() + (time_t)NanoManager::SkillTable[skillId].durationTime[0] * 25;
    EggBuffs[key] = until;

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
        else { // if time reached 0
            CNSocket* sock = it->first.first;
            int32_t CBFlag = it->first.second;
            Player* plr = PlayerManager::getPlayer(sock);
            Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            int groupFlags = GroupManager::getGroupFlags(otherPlr);
            for (auto& pwr : NanoManager::NanoPowers) {
                if (pwr.bitFlag == CBFlag) { // pick the power with the right flag and unbuff
                    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
                    resp.eCSTB = pwr.timeBuffID;
                    resp.eTBU = 2;
                    resp.eTBT = 3; // for egg buffs
                    plr->iConditionBitFlag &= ~CBFlag;
                    resp.iConditionBitFlag = plr->iConditionBitFlag |= groupFlags | plr->iSelfConditionBitFlag;
                    sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

                    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, resp2); // send a buff timeout to other players
                    resp2.eCT = 1;
                    resp2.iID = plr->iID;
                    resp2.iConditionBitFlag = plr->iConditionBitFlag;
                    PlayerManager::sendToViewable(sock, (void*)&resp2, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
                }
            }
            // remove buff from the map
            it = EggBuffs.erase(it);
        }
    }

    // check dead eggs and eggs in inactive chunks
    for (auto egg : Eggs) {
        if (!egg.second->dead || !ChunkManager::inPopulatedChunks(egg.second->viewableChunks))
            continue;
        if (egg.second->deadUntil <= timeStamp) {
            // respawn it
            egg.second->dead = false;
            egg.second->deadUntil = 0;
            egg.second->appearanceData.iHP = 400;
            
            ChunkManager::addNPCToChunks(ChunkManager::getViewableChunks(egg.second->chunkPos), egg.first);
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

    /* this has some issues with position desync, leaving it out for now
    if (abs(egg->appearanceData.iX - plr->x)>500 || abs(egg->appearanceData.iY - plr->y) > 500) {
        std::cout << "[WARN] Player tried to open an egg from the other chunk?!" << std::endl;
        return;
    }
    */

    int typeId = egg->appearanceData.iNPCType;
    if (EggTypes.find(typeId) == EggTypes.end()) {
        std::cout << "[WARN] Egg Type " << typeId << " not found!" << std::endl;
        return;
    }

    EggType* type = &EggTypes[typeId];

    // buff the player
    if (type->effectId != 0)
        eggBuffPlayer(sock, type->effectId, eggId);

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
        ChunkManager::removeNPCFromChunks(ChunkManager::getViewableChunks(egg->chunkPos), eggId);
        egg->dead = true;
        egg->deadUntil = getTime() + (time_t)type->regen * 1000;
        egg->appearanceData.iHP = 0;
    }
}

#pragma region NPCEvents

// summon right arm and stage 2 body
static void lordFuseStageTwo(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc; // adaptium, stun
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage two" << std::endl;

    // Fuse doesn't move; spawnX, etc. is shorter to write than *appearanceData*
    // Blastons, Heal
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2467);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // right arm, Adaptium, Stun
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX - 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2469);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

// summon left arm and stage 3 body
static void lordFuseStageThree(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc;
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage three" << std::endl;

    // Cosmic, Damage Point
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2468);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // Blastons, Heal
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX + 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2470);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

std::vector<NPCEvent> NPCManager::NPCEvents = {
    NPCEvent(2466, ON_KILLED, lordFuseStageTwo),
    NPCEvent(2467, ON_KILLED, lordFuseStageThree),
};

#pragma endregion NPCEvents
