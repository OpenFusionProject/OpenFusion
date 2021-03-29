#include "servers/CNShardServer.hpp"
#include "Items.hpp"
#include "PlayerManager.hpp"
#include "Nanos.hpp"
#include "NPCManager.hpp"
#include "Player.hpp"
#include "Abilities.hpp"
#include "Missions.hpp"
#include "Eggs.hpp"
#include "Rand.hpp"

#include <string.h> // for memset()
#include <assert.h>

using namespace Items;

std::map<std::pair<int32_t, int32_t>, Items::Item> Items::ItemData;
std::map<int32_t, CrocPotEntry> Items::CrocPotTable;
std::map<int32_t, std::vector<int32_t>> Items::RarityWeights;
std::map<int32_t, Crate> Items::Crates;
std::map<int32_t, ItemReference> Items::ItemReferences;
std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> Items::CodeItems;

std::map<int32_t, CrateDropChance> Items::CrateDropChances;
std::map<int32_t, std::vector<int32_t>> Items::CrateDropTypes;
std::map<int32_t, MiscDropChance> Items::MiscDropChances;
std::map<int32_t, MiscDropType> Items::MiscDropTypes;
std::map<int32_t, MobDrop> Items::MobDrops;
std::map<int32_t, ItemSetType> Items::ItemSetTypes;
std::map<int32_t, ItemSetChance> Items::ItemSetChances;

#ifdef ACADEMY
std::map<int32_t, int32_t> Items::NanoCapsules; // crate id -> nano id

static void nanoCapsuleHandler(CNSocket* sock, int slot, sItemBase *chest) {
    Player* plr = PlayerManager::getPlayer(sock);
    int32_t nanoId = NanoCapsules[chest->iID];

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);
    resp.iSlotNum = slot;

    // in order to remove capsule form inventory, we have to send item reward packet with empty item
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);

    // we know it's only one trailing struct, so we can skip full validation
    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
    sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // maintain stats
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;

    item->iSlotNum = slot;
    item->eIL = 1;

    // update player serverside
    plr->Inven[slot] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // transmit chest opening acknowledgement packet
    sock->sendPacket(resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC);

    // check if player doesn't already have this nano
    if (plr->Nanos[nanoId].iID != 0) {
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        msg.iDuringTime = 4;
        std::string text = "You have already acquired this nano!";
        U8toU16(text, msg.szAnnounceMsg, sizeof(text));
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        return;
    }
    Nanos::addNano(sock, nanoId, -1, false);
}
#endif

static int choice(const std::vector<int>& weights, int rolled) {
    int total = std::accumulate(weights.begin(), weights.end(), 0);
    int randValue = rolled % total;
    int currentIndex = -1;

    do {
        currentIndex++;
        randValue -= weights[currentIndex];
    } while (randValue >= 0);

    return currentIndex;
}

static int getRarity(int crateId, int itemSetTypeId) {
    Crate& crate = Items::Crates[crateId];

    // find rarity ratio
    if (Items::RarityWeights.find(crate.rarityWeightId) == Items::RarityWeights.end()) {
        std::cout << "[WARN] Rarity Weight " << crate.rarityWeightId << " not found!" << std::endl;
        return -1;
    }

    std::vector<int>& rarityWeights = Items::RarityWeights[crate.rarityWeightId];

    /*
     * First we have to check if specified item set contains items with all specified rarities,
     * and if not eliminate them from the draw
     * it is simpler to do here than to fix individually in the file
     */

    // remember that rarities start from 1!
    std::set<int> rarityIndices;
    for (int itemReferenceId : Items::ItemSetTypes[itemSetTypeId].itemReferenceIds) {
        if (Items::ItemReferences.find(itemReferenceId) == Items::ItemReferences.end())
            continue;

        rarityIndices.insert(Items::ItemReferences[itemReferenceId].rarity - 1);

        // shortcut
        if (rarityIndices.size() == rarityWeights.size())
            break;
    }

    if (rarityIndices.empty()) {
        std::cout << "[WARN] Item Set " << crate.itemSetTypeId << " has no valid items assigned?!" << std::endl;
        return -1;
    }

    std::vector<int> relevantWeights;
    for (int index : rarityIndices)
        relevantWeights.push_back(rarityWeights[index]);

    // now return a random rarity number (starting from 1)
    return Rand::randWeighted(relevantWeights) + 1;
}

static int getCrateItem(sItemBase* result, int itemSetTypeId, int itemSetChanceId, int rarity, int playerGender) {
    // (int, vector<int>)
    auto& [ignoreGender, itemReferenceIds] = Items::ItemSetTypes[itemSetTypeId];

    // collect valid items that match the rarity and (if not ignored) gender
    std::vector<std::pair<int, ItemReference*>> validItems;
    for (int i = 0; i < itemReferenceIds.size(); i++) {
        int itemReferenceId = itemReferenceIds[i];

        if (Items::ItemReferences.find(itemReferenceId) == Items::ItemReferences.end()) {
            std::cout << "[WARN] Item reference " << itemReferenceId << " in item set type "
                      << itemSetTypeId << " was not found, skipping..." << std::endl;
            continue;
        }

        ItemReference* item = &Items::ItemReferences[itemReferenceId];

        if (item->rarity != rarity)
            continue;

        // if gender is incorrect, exclude item
        if (!ignoreGender && item->gender != 0 && item->gender != playerGender)
            continue;

        validItems.push_back(std::make_pair(i, item));
    }

    if (validItems.empty()) {
        std::cout << "[WARN] Set ID " << itemSetTypeId << " Chance ID " << itemSetChanceId
                  << " Rarity " << rarity << " contains no valid items" << std::endl;
        return -1;
    }

    // (int, map<int, int>)
    auto& [defaultWeight, indexWeightMap] = Items::ItemSetChances[itemSetChanceId];

    // initialize all weights as the default weight for all item slots
    std::vector<int> itemWeights(validItems.size(), defaultWeight);

    if (!indexWeightMap.empty()) {
        for (int i = 0; i < validItems.size(); i++) {
            int dropIndex = validItems[i].first;

            if (indexWeightMap.find(dropIndex) == indexWeightMap.end())
                continue;

            int weight = indexWeightMap[dropIndex];
            // allow 0 weights for convenience
            if (weight > -1)
                itemWeights[i] = weight;
        }
    }

    int chosenIndex = Rand::randWeighted(itemWeights);
    ItemReference* item = validItems[chosenIndex].second;

    result->iID = item->itemId;
    result->iType = item->type;
    result->iOpt = 1;

    return 0;
}

static int getValidCrateId(int crateId) {
    // find the crate
    if (Items::Crates.find(crateId) == Items::Crates.end()) {
        std::cout << "[WARN] Crate " << crateId << " not found!" << std::endl;
        return -1;
    }

    return crateId;
}

static int getValidItemSetTypeId(int crateId) {
    Crate& crate = Items::Crates[crateId];

    // find item set type
    if (Items::ItemSetTypes.find(crate.itemSetTypeId) == Items::ItemSetTypes.end()) {
        std::cout << "[WARN] Crate " << crateId << " was assigned item set "
                  << crate.itemSetTypeId << " which is invalid!" << std::endl;
        return -1;
    }

    return crate.itemSetTypeId;
}

static int getValidItemSetChanceId(int crateId) {
    Crate& crate = Items::Crates[crateId];

    // find item set chances
    if (Items::ItemSetChances.find(crate.itemSetChanceId) == Items::ItemSetChances.end()) {
        std::cout << "[WARN] Crate " << crateId << " was assigned item set chance object "
                  << crate.itemSetChanceId << " which is invalid!" << std::endl;
        return -1;
    }

    return crate.itemSetChanceId;
}

static void itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    auto itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    INITSTRUCT(sP_FE2CL_PC_ITEM_MOVE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (itemmove->iToSlotNum < 0 || itemmove->iFromSlotNum < 0)
        return;
    // NOTE: sending a no-op, "move in-place" packet is not necessary

    if (plr->isTrading) {
        std::cout << "[WARN] Player attempted to move item while trading" << std::endl;
        return;
    }

    // get the fromItem
    sItemBase *fromItem;
    switch ((SlotType)itemmove->eFrom) {
    case SlotType::EQUIP:
        if (itemmove->iFromSlotNum >= AEQUIP_COUNT)
            return;

        fromItem = &plr->Equip[itemmove->iFromSlotNum];
        break;
    case SlotType::INVENTORY:
        if (itemmove->iFromSlotNum >= AINVEN_COUNT)
            return;

        fromItem = &plr->Inven[itemmove->iFromSlotNum];
        break;
    case SlotType::BANK:
        if (itemmove->iFromSlotNum >= ABANK_COUNT)
            return;

        fromItem = &plr->Bank[itemmove->iFromSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eFrom << std::endl;
        return;
    }

    // get the toItem
    sItemBase* toItem;
    switch ((SlotType)itemmove->eTo) {
    case SlotType::EQUIP:
        if (itemmove->iToSlotNum >= AEQUIP_COUNT)
            return;

        toItem = &plr->Equip[itemmove->iToSlotNum];
        break;
    case SlotType::INVENTORY:
        if (itemmove->iToSlotNum >= AINVEN_COUNT)
            return;

        toItem = &plr->Inven[itemmove->iToSlotNum];
        break;
    case SlotType::BANK:
        if (itemmove->iToSlotNum >= ABANK_COUNT)
            return;

        toItem = &plr->Bank[itemmove->iToSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eTo << std::endl;
        return;
    }

    // if equipping an item, validate that it's of the correct type for the slot
    if ((SlotType)itemmove->eTo == SlotType::EQUIP) {
        if (fromItem->iType == 10 && itemmove->iToSlotNum != 8)
            return; // vehicle in wrong slot
        else if (fromItem->iType != 10
              && !(fromItem->iType == 0 && itemmove->iToSlotNum == 7)
              && fromItem->iType != itemmove->iToSlotNum)
            return; // something other than a vehicle or a weapon in a non-matching slot
        else if (itemmove->iToSlotNum >= AEQUIP_COUNT) // TODO: reject slots >= 9?
            return; // invalid slot
    }

    // save items to response
    resp.eTo = itemmove->eFrom;
    resp.eFrom = itemmove->eTo;
    resp.ToSlotItem = *toItem;
    resp.FromSlotItem = *fromItem;

    // swap/stack items in session
    Item* itemDat = getItemData(toItem->iID, toItem->iType);
    Item* itemDatFrom = getItemData(fromItem->iID, fromItem->iType);
    if (itemDat != nullptr && itemDatFrom != nullptr && itemDat->stackSize > 1 && itemDat == itemDatFrom && fromItem->iOpt < itemDat->stackSize && toItem->iOpt < itemDat->stackSize) {
        // items are stackable, identical, and not maxed, so run stacking logic

        toItem->iOpt += fromItem->iOpt; // sum counts
        fromItem->iOpt = 0; // deplete from item
        if (toItem->iOpt > itemDat->stackSize) {
            // handle overflow
            fromItem->iOpt += (toItem->iOpt - itemDat->stackSize); // add overflow to fromItem
            toItem->iOpt = itemDat->stackSize; // set toItem count to max
        }

        if (fromItem->iOpt == 0) { // from item count depleted
            // delete item
            fromItem->iID = 0;
            fromItem->iType = 0;
            fromItem->iTimeLimit = 0;
        }

        resp.iFromSlotNum = itemmove->iFromSlotNum;
        resp.iToSlotNum = itemmove->iToSlotNum;
        resp.FromSlotItem = *fromItem;
        resp.ToSlotItem = *toItem;
    } else {
        // items not stackable; just swap them
        sItemBase temp = *toItem;
        *toItem = *fromItem;
        *fromItem = temp;
        resp.iFromSlotNum = itemmove->iToSlotNum;
        resp.iToSlotNum = itemmove->iFromSlotNum;
    }

    // send equip change to viewable players
    if (itemmove->eFrom == (int)SlotType::EQUIP || itemmove->eTo == (int)SlotType::EQUIP) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, equipChange);

        equipChange.iPC_ID = plr->iID;
        if (itemmove->eTo == (int)SlotType::EQUIP) {
            equipChange.iEquipSlotNum = itemmove->iToSlotNum;
            equipChange.EquipSlotItem = resp.FromSlotItem;
        } else {
            equipChange.iEquipSlotNum = itemmove->iFromSlotNum;
            equipChange.EquipSlotItem = resp.ToSlotItem;
        }

        // unequip vehicle if equip slot 8 is 0
        if (plr->Equip[8].iID == 0 && plr->iPCState & 8) {
            INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, response);
            sock->sendPacket(response, P_FE2CL_PC_VEHICLE_OFF_SUCC);

            // send to other players
            plr->iPCState &= ~8;
            INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, response2);
            response2.iPC_ID = plr->iID;
            response2.iState = plr->iPCState;

            PlayerManager::sendToViewable(sock, response2, P_FE2CL_PC_STATE_CHANGE);
        }

        // send equip event to other players
        PlayerManager::sendToViewable(sock, equipChange, P_FE2CL_PC_EQUIP_CHANGE);

        // set equipment stats serverside
        setItemStats(plr);
    }

    // send response
    sock->sendPacket(resp, P_FE2CL_PC_ITEM_MOVE_SUCC);
}

static void itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    auto itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    resp.eIL = itemdel->eIL;
    resp.iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr->Inven[itemdel->iSlotNum].iID = 0;
    plr->Inven[itemdel->iSlotNum].iType = 0;
    plr->Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket(resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC);
}

static void itemUseHandler(CNSocket* sock, CNPacketData* data) {
    auto request = (sP_CL2FE_REQ_ITEM_USE*)data->buf;
    Player* player = PlayerManager::getPlayer(sock);

    if (request->iSlotNum < 0 || request->iSlotNum >= AINVEN_COUNT)
        return; // sanity check

    // gumball can only be used from inventory, so we ignore eIL
    sItemBase gumball = player->Inven[request->iSlotNum];
    sNano nano = player->Nanos[player->equippedNanos[request->iNanoSlot]];

    // sanity check, check if gumball exists
    if (!(gumball.iOpt > 0 && gumball.iType == 7 && gumball.iID>=119 && gumball.iID<=121)) {
        std::cout << "[WARN] Gumball not found" << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_FAIL, response);
        sock->sendPacket(response, P_FE2CL_REP_PC_ITEM_USE_FAIL);
        return;
    }

    // sanity check, check if gumball type matches nano style
    int nanoStyle = Nanos::nanoStyle(nano.iID);
    if (!((gumball.iID == 119 && nanoStyle == 0) ||
        (  gumball.iID == 120 && nanoStyle == 1) ||
        (  gumball.iID == 121 && nanoStyle == 2))) {
        std::cout << "[WARN] Gumball type doesn't match nano type" << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_FAIL, response);
        sock->sendPacket(response, P_FE2CL_REP_PC_ITEM_USE_FAIL);
        return;
    }

    gumball.iOpt -= 1;
    if (gumball.iOpt == 0)
        gumball = {};

    size_t resplen = sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC) + sizeof(sSkillResult_Buff);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC), 1, sizeof(sSkillResult_Buff))) {
        std::cout << "[WARN] bad sP_FE2CL_REP_PC_ITEM_USE_SUCC packet size" << std::endl;
        return;
    }

    if (gumball.iOpt == 0)
        gumball = {};

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_REP_PC_ITEM_USE_SUCC *resp = (sP_FE2CL_REP_PC_ITEM_USE_SUCC*)respbuf;
    sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));
    resp->iPC_ID = player->iID;
    resp->eIL = 1;
    resp->iSlotNum = request->iSlotNum;
    resp->RemainItem = gumball;
    resp->iTargetCnt = 1;
    resp->eST = EST_NANOSTIMPAK;
    resp->iSkillID = 144;

    int value1 = CSB_BIT_STIMPAKSLOT1 << request->iNanoSlot;
    int value2 = ECSB_STIMPAKSLOT1 + request->iNanoSlot;

    respdata->eCT = 1;
    respdata->iID = player->iID;
    respdata->iConditionBitFlag = value1;

    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
    pkt.eCSTB = value2; // eCharStatusTimeBuffID
    pkt.eTBU = 1; // eTimeBuffUpdate
    pkt.eTBT = 1; // eTimeBuffType 1 means nano
    pkt.iConditionBitFlag = player->iConditionBitFlag |= value1;
    sock->sendPacket(pkt, P_FE2CL_PC_BUFF_UPDATE);

    sock->sendPacket((void*)&respbuf, P_FE2CL_REP_PC_ITEM_USE_SUCC, resplen);
    // update inventory serverside
    player->Inven[resp->iSlotNum] = resp->RemainItem;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, value1);
    time_t until = getTime() + (time_t)Nanos::SkillTable[144].durationTime[0] * 100;
    Eggs::EggBuffs[key] = until;
}

static void itemBankOpenHandler(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);

    // just send bank inventory
    INITSTRUCT(sP_FE2CL_REP_PC_BANK_OPEN_SUCC, resp);
    for (int i = 0; i < ABANK_COUNT; i++) {
        resp.aBank[i] = plr->Bank[i];
    }
    resp.iExtraBank = 1;
    sock->sendPacket(resp, P_FE2CL_REP_PC_BANK_OPEN_SUCC);
}

static void chestOpenHandler(CNSocket *sock, CNPacketData *data) {
    auto pkt = (sP_CL2FE_REQ_ITEM_CHEST_OPEN *)data->buf;

    // sanity check
    if (pkt->eIL != 1 || pkt->iSlotNum < 0 || pkt->iSlotNum >= AINVEN_COUNT)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    sItemBase *chest = &plr->Inven[pkt->iSlotNum];
    // we could reject the packet if the client thinks the item is different, but eh

    if (chest->iType != 9) {
        std::cout << "[WARN] Player tried to open a crate with incorrect iType ?!" << std::endl;
        return;
    }

#ifdef ACADEMY
    // check if chest isn't a nano capsule
    if (NanoCapsules.find(chest->iID) != NanoCapsules.end())
        return nanoCapsuleHandler(sock, pkt->iSlotNum, chest);
#endif

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);
    resp.iSlotNum = pkt->iSlotNum;

    // item giving packet
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // maintain stats
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;

    item->iSlotNum = pkt->iSlotNum;
    item->eIL = 1;

    int validItemSetTypeId = -1, validItemSetChanceId = -1, rarity = -1, ret = -1;

    int validCrateId = getValidCrateId(chest->iID);
    bool failing = (validCrateId == -1);

    if (!failing)
        validItemSetTypeId = getValidItemSetTypeId(validCrateId);
    failing = (validItemSetTypeId == -1);

    if (!failing)
        validItemSetChanceId = getValidItemSetChanceId(validCrateId);
    failing = (validItemSetChanceId == -1);

    if (!failing)
        rarity = getRarity(validCrateId, validItemSetTypeId);
    failing = (rarity == -1);

    if (!failing)
        ret = getCrateItem(&item->sItem, validItemSetTypeId, validItemSetChanceId, rarity, plr->PCStyle.iGender);
    failing = (ret == -1);

    // if we failed to open a crate, at least give the player a gumball (suggested by Jade)
    if (failing) {
        item->sItem.iType = 7;
        item->sItem.iID = 119 + Rand::rand(3);
        item->sItem.iOpt = 1;

        std::cout << "[WARN] Crate open failed, giving a Gumball..." << std::endl;
    }
    // update player
    plr->Inven[pkt->iSlotNum] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // transmit chest opening acknowledgement packet
    std::cout << "opening chest..." << std::endl;
    sock->sendPacket(resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC);
}

// TODO: use this in cleaned up Items
int Items::findFreeSlot(Player *plr) {
    int i;

    for (i = 0; i < AINVEN_COUNT; i++)
        if (plr->Inven[i].iType == 0 && plr->Inven[i].iID == 0 && plr->Inven[i].iOpt == 0)
            return i;

    // not found
    return -1;
}

Item* Items::getItemData(int32_t id, int32_t type) {
    if(ItemData.find(std::make_pair(id, type)) !=  ItemData.end())
        return &ItemData[std::make_pair(id, type)];
    return nullptr;
}

void Items::checkItemExpire(CNSocket* sock, Player* player) {
    if (player->toRemoveVehicle.eIL == 0 && player->toRemoveVehicle.iSlotNum == 0)
        return;

    /* prepare packet
    * yes, this is a varadic packet, however analyzing client behavior and code
    * it only checks takes the first item sent into account
    * yes, this is very stupid
    * therefore, we delete all but 1 expired vehicle while loading player
    * to delete the last one here so player gets a notification
    */

    const size_t resplen = sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM) + sizeof(sTimeLimitItemDeleteInfo2CL);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation
    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    auto packet = (sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM*)respbuf;
    sTimeLimitItemDeleteInfo2CL* itemData = (sTimeLimitItemDeleteInfo2CL*)(respbuf + sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM));
    memset(respbuf, 0, resplen);

    packet->iItemListCount = 1;
    itemData->eIL = player->toRemoveVehicle.eIL;
    itemData->iSlotNum = player->toRemoveVehicle.iSlotNum;
    sock->sendPacket((void*)&respbuf, P_FE2CL_PC_DELETE_TIME_LIMIT_ITEM, resplen);

    // delete serverside
    if (player->toRemoveVehicle.eIL == 0)
        memset(&player->Equip[8], 0, sizeof(sItemBase));
    else
        memset(&player->Inven[player->toRemoveVehicle.iSlotNum], 0, sizeof(sItemBase));

    player->toRemoveVehicle.eIL = 0;
    player->toRemoveVehicle.iSlotNum = 0;
}

void Items::setItemStats(Player* plr) {

    plr->pointDamage = 8 + plr->level * 2;
    plr->groupDamage = 8 + plr->level * 2;
    plr->fireRate = 0;
    plr->defense = 16 + plr->level * 4;

    Item* itemStatsDat;

    for (int i = 0; i < 4; i++) {
        itemStatsDat = getItemData(plr->Equip[i].iID, plr->Equip[i].iType);
        if (itemStatsDat == nullptr) {
            std::cout << "[WARN] setItemStats(): getItemData() returned NULL" << std::endl;
            continue;
        }
        plr->pointDamage += itemStatsDat->pointDamage;
        plr->groupDamage += itemStatsDat->groupDamage;
        plr->fireRate += itemStatsDat->fireRate;
        plr->defense += itemStatsDat->defense;
    }
}

// HACK: work around the invisible weapon bug
// TODO: I don't think this makes a difference at all? Check and remove, if necessary.
void Items::updateEquips(CNSocket* sock, Player* plr) {
    for (int i = 0; i < 4; i++) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, resp);

        resp.iPC_ID = plr->iID;
        resp.iEquipSlotNum = i;
        resp.EquipSlotItem = plr->Equip[i];

        PlayerManager::sendToViewable(sock, resp, P_FE2CL_PC_EQUIP_CHANGE);
    }
}

static void getMobDrop(sItemBase* reward, const std::vector<int>& weights, const std::vector<int>& crateIds, int rolled) {
    int chosenIndex = choice(weights, rolled);

    reward->iType = 9;
    reward->iOpt = 1;
    reward->iID = crateIds[chosenIndex];
}

static void giveEventDrop(CNSocket* sock, Player* player, int rolled) {
    // random drop chance
    if (Rand::rand(100) > settings::EVENTCRATECHANCE)
        return;

    // no slot = no reward
    int slot = findFreeSlot(player);
    if (slot == -1)
        return;

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);

    uint8_t respbuf[resplen];
    sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
    sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // leave everything here as it is
    reward->m_iCandy = player->money;
    reward->m_iFusionMatter = player->fusionmatter;
    reward->m_iBatteryN = player->batteryN;
    reward->m_iBatteryW = player->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    // which crate to drop
    int crateId;
    switch (settings::EVENTMODE) {
    // knishmas
    case 1: crateId = 1187; break;
    // halloween
    case 2: crateId = 1181; break;
    // spring
    case 3: crateId = 1126; break;
    // what
    default:
        std::cout << "[WARN] Unknown event Id " << settings::EVENTMODE << std::endl;
        return;
    }

    item->sItem.iType = 9;
    item->sItem.iID = crateId;
    item->sItem.iOpt = 1;
    item->iSlotNum = slot;
    item->eIL = 1; // Inventory Location. 1 means player inventory.

    // update player
    player->Inven[slot] = item->sItem;
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
}

void Items::giveMobDrop(CNSocket *sock, Mob* mob, int rolledBoosts, int rolledPotions,
                            int rolledCrate, int rolledCrateType, int rolledEvent) {
    Player *plr = PlayerManager::getPlayer(sock);

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // sanity check
    if (MobDrops.find(mob->dropType) == MobDrops.end()) {
        std::cout << "[WARN] Drop Type " << mob->dropType << " was not found" << std::endl;
        return;
    }
    // find correct mob drop
    MobDrop& drop = MobDrops[mob->dropType];

    // use the keys to fetch data from other maps
    // sanity check
    if (Items::CrateDropChances.find(drop.crateDropChanceId) == Items::CrateDropChances.end()) {
        std::cout << "[WARN] Crate Drop Chance Object " << drop.crateDropChanceId << " was not found" << std::endl;
        return;
    }
    CrateDropChance& crateDropChance = Items::CrateDropChances[drop.crateDropChanceId];

    // sanity check
    if (Items::CrateDropTypes.find(drop.crateDropTypeId) == Items::CrateDropTypes.end()) {
        std::cout << "[WARN] Crate Drop Type Object " << drop.crateDropTypeId << " was not found" << std::endl;
        return;
    }
    std::vector<int>& crateDropType = Items::CrateDropTypes[drop.crateDropTypeId];

    // sanity check
    if (Items::MiscDropChances.find(drop.miscDropChanceId) == Items::MiscDropChances.end()) {
        std::cout << "[WARN] Misc Drop Chance Object " << drop.miscDropChanceId << " was not found" << std::endl;
        return;
    }
    MiscDropChance& miscDropChance = Items::MiscDropChances[drop.miscDropChanceId];

    // sanity check
    if (Items::MiscDropTypes.find(drop.miscDropTypeId) == Items::MiscDropTypes.end()) {
        std::cout << "[WARN] Misc Drop Type Object " << drop.miscDropTypeId << " was not found" << std::endl;
        return;
    }
    MiscDropType& miscDropType = Items::MiscDropTypes[drop.miscDropTypeId];

    plr->money += miscDropType.taroAmount;
    // money nano boost
    if (plr->iConditionBitFlag & CSB_BIT_REWARD_CASH) {
        int boost = 0;
        if (Nanos::getNanoBoost(plr)) // for gumballs
            boost = 1;
        plr->money += miscDropType.taroAmount * (5 + boost) / 25;
    }
    // formula for scaling FM with player/mob level difference
    // TODO: adjust this better
    int levelDifference = plr->level - mob->level;
    int fm = miscDropType.fmAmount;
    if (levelDifference > 0)
        fm = levelDifference < 10 ? fm - (levelDifference * fm / 10) : 0;
    // scavenger nano boost
    if (plr->iConditionBitFlag & CSB_BIT_REWARD_BLOB) {
        int boost = 0;
        if (Nanos::getNanoBoost(plr)) // for gumballs
            boost = 1;
        fm += fm * (5 + boost) / 25;
    }

    Missions::updateFusionMatter(sock, fm);

    if (rolledPotions % miscDropChance.potionDropChanceTotal < miscDropChance.potionDropChance)
        plr->batteryN += miscDropType.potionAmount;
    if (rolledBoosts % miscDropChance.boostDropChanceTotal < miscDropChance.boostDropChance)
        plr->batteryW += miscDropType.boostAmount;

    // caps
    if (plr->batteryW > 9999)
        plr->batteryW = 9999;
    if (plr->batteryN > 9999)
        plr->batteryN = 9999;

    // simple rewards
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    int slot = findFreeSlot(plr);

    // no drop
    if (slot == -1 || rolledCrate % crateDropChance.dropChanceTotal >= crateDropChance.dropChance) {
        // no room for an item, but you still get FM and taros
        reward->iItemCnt = 0;
        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, sizeof(sP_FE2CL_REP_REWARD_ITEM));
    } else {
        // item reward
        getMobDrop(&item->sItem, crateDropChance.crateTypeDropWeights, crateDropType, rolledCrateType);
        item->iSlotNum = slot;
        item->eIL = 1; // Inventory Location. 1 means player inventory.

        // update player
        plr->Inven[slot] = item->sItem;

        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    }

    // event crates
    if (settings::EVENTMODE != 0)
        giveEventDrop(sock, plr, rolledEvent);
}

void Items::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    // this one is for gumballs
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_USE, itemUseHandler);
    // Bank
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BANK_OPEN, itemBankOpenHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
}
