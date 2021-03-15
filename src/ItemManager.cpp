#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "Player.hpp"
#include "Abilities.hpp"

#include <string.h> // for memset()
#include <assert.h>

std::map<std::pair<int32_t, int32_t>, ItemManager::Item> ItemManager::ItemData;
std::map<int32_t, CrocPotEntry> ItemManager::CrocPotTable;
std::map<int32_t, std::vector<int>> ItemManager::RarityRatios;
std::map<int32_t, Crate> ItemManager::Crates;
// pair Itemset, Rarity -> vector of pointers (map iterators) to records in ItemData
std::map<std::pair<int32_t, int32_t>, std::vector<std::map<std::pair<int32_t, int32_t>, ItemManager::Item>::iterator>> ItemManager::CrateItems;
std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> ItemManager::CodeItems;

#ifdef ACADEMY
std::map<int32_t, int32_t> ItemManager::NanoCapsules; // crate id -> nano id

void nanoCapsuleHandler(CNSocket* sock, int slot, sItemBase *chest) {
    Player* plr = PlayerManager::getPlayer(sock);
    int32_t nanoId = ItemManager::NanoCapsules[chest->iID];

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
    sock->sendPacket((void*)&resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC));

    // check if player doesn't already have this nano
    if (plr->Nanos[nanoId].iID != 0) {
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        msg.iDuringTime = 4;
        std::string text = "You have already aquired this nano!";
        U8toU16(text, msg.szAnnounceMsg, sizeof(text));
        sock->sendPacket((void*)&msg, P_FE2CL_GM_REP_PC_ANNOUNCE, sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE));
        return;
    }
    NanoManager::addNano(sock, nanoId, -1, false);
}
#endif

void itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
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
    switch ((ItemManager::SlotType)itemmove->eFrom) {
    case ItemManager::SlotType::EQUIP:
        if (itemmove->iFromSlotNum >= AEQUIP_COUNT)
            return;

        fromItem = &plr->Equip[itemmove->iFromSlotNum];
        break;
    case ItemManager::SlotType::INVENTORY:
        if (itemmove->iFromSlotNum >= AINVEN_COUNT)
            return;

        fromItem = &plr->Inven[itemmove->iFromSlotNum];
        break;
    case ItemManager::SlotType::BANK:
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
    switch ((ItemManager::SlotType)itemmove->eTo) {
    case ItemManager::SlotType::EQUIP:
        if (itemmove->iToSlotNum >= AEQUIP_COUNT)
            return;

        toItem = &plr->Equip[itemmove->iToSlotNum];
        break;
    case ItemManager::SlotType::INVENTORY:
        if (itemmove->iToSlotNum >= AINVEN_COUNT)
            return;

        toItem = &plr->Inven[itemmove->iToSlotNum];
        break;
    case ItemManager::SlotType::BANK:
        if (itemmove->iToSlotNum >= ABANK_COUNT)
            return;

        toItem = &plr->Bank[itemmove->iToSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eTo << std::endl;
        return;
    }

    // if equipping an item, validate that it's of the correct type for the slot
    if ((ItemManager::SlotType)itemmove->eTo == ItemManager::SlotType::EQUIP) {
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
    ItemManager::Item* itemDat = ItemManager::getItemData(toItem->iID, toItem->iType);
    ItemManager::Item* itemDatFrom = ItemManager::getItemData(fromItem->iID, fromItem->iType);
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
    if (itemmove->eFrom == (int)ItemManager::SlotType::EQUIP || itemmove->eTo == (int)ItemManager::SlotType::EQUIP) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, equipChange);

        equipChange.iPC_ID = plr->iID;
        if (itemmove->eTo == (int)ItemManager::SlotType::EQUIP) {
            equipChange.iEquipSlotNum = itemmove->iToSlotNum;
            equipChange.EquipSlotItem = resp.FromSlotItem;
        } else {
            equipChange.iEquipSlotNum = itemmove->iFromSlotNum;
            equipChange.EquipSlotItem = resp.ToSlotItem;
        }

        // unequip vehicle if equip slot 8 is 0
        if (plr->Equip[8].iID == 0)
            plr->iPCState = 0;

        // send equip event to other players
        PlayerManager::sendToViewable(sock, (void*)&equipChange, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));

        // set equipment stats serverside
        ItemManager::setItemStats(plr);
    }

    // send response
    sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
}

void itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ITEM_DELETE* itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    resp.eIL = itemdel->eIL;
    resp.iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr->Inven[itemdel->iSlotNum].iID = 0;
    plr->Inven[itemdel->iSlotNum].iType = 0;
    plr->Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));
}

void itemUseHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_USE))
        return; // ignore the malformed packet
    sP_CL2FE_REQ_ITEM_USE* request = (sP_CL2FE_REQ_ITEM_USE*)data->buf;
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
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ITEM_USE_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL));
        return;
    }

    // sanity check, check if gumball type matches nano style
    int nanoStyle = NanoManager::nanoStyle(nano.iID);
    if (!((gumball.iID == 119 && nanoStyle == 0) ||
        (  gumball.iID == 120 && nanoStyle == 1) ||
        (  gumball.iID == 121 && nanoStyle == 2))) {
        std::cout << "[WARN] Gumball type doesn't match nano type" << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_ITEM_USE_FAIL, response);
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ITEM_USE_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL));
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
    sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

    sock->sendPacket((void*)&respbuf, P_FE2CL_REP_PC_ITEM_USE_SUCC, resplen);
    // update inventory serverside
    player->Inven[resp->iSlotNum] = resp->RemainItem;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, value1);
    time_t until = getTime() + (time_t)NanoManager::SkillTable[144].durationTime[0] * 100;
    NPCManager::EggBuffs[key] = until;
}

void itemBankOpenHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_BANK_OPEN))
        return; // ignore the malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    // just send bank inventory
    INITSTRUCT(sP_FE2CL_REP_PC_BANK_OPEN_SUCC, resp);
    for (int i = 0; i < ABANK_COUNT; i++) {
        resp.aBank[i] = plr->Bank[i];
    }
    resp.iExtraBank = 1;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_BANK_OPEN_SUCC, sizeof(sP_FE2CL_REP_PC_BANK_OPEN_SUCC));
}

void itemCombineHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_COMBINATION))
        return; // malformed packet

    sP_CL2FE_REQ_PC_ITEM_COMBINATION* req = (sP_CL2FE_REQ_PC_ITEM_COMBINATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // prepare fail packet
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, failResp);
    failResp.iCostumeItemSlot = req->iCostumeItemSlot;
    failResp.iStatItemSlot = req->iStatItemSlot;
    failResp.iErrorCode = 0;

    // sanity check slot indices
    if (req->iCostumeItemSlot < 0 || req->iCostumeItemSlot >= AINVEN_COUNT || req->iStatItemSlot < 0 || req->iStatItemSlot >= AINVEN_COUNT) {
        std::cout << "[WARN] Inventory slot(s) out of range (" << req->iStatItemSlot << " and " << req->iCostumeItemSlot << ")" << std::endl;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL));
        return;
    }

    sItemBase* itemStats = &plr->Inven[req->iStatItemSlot];
    sItemBase* itemLooks = &plr->Inven[req->iCostumeItemSlot];
    ItemManager::Item* itemStatsDat = ItemManager::getItemData(itemStats->iID, itemStats->iType);
    ItemManager::Item* itemLooksDat = ItemManager::getItemData(itemLooks->iID, itemLooks->iType);

    // sanity check item and combination entry existence
    if (itemStatsDat == nullptr || itemLooksDat == nullptr
        || ItemManager::CrocPotTable.find(abs(itemStatsDat->level - itemLooksDat->level)) == ItemManager::CrocPotTable.end()) {
        std::cout << "[WARN] Either item ids or croc pot value set not found" << std::endl;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL));
        return;
    }

    // sanity check matching item types
    if (itemStats->iType != itemLooks->iType
        || (itemStats->iType == 0 && itemStatsDat->weaponType != itemLooksDat->weaponType)) {
        std::cout << "[WARN] Player attempted to combine mismatched items" << std::endl;
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_ITEM_COMBINATION_FAIL, sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL));
        return;
    }

    CrocPotEntry* recipe = &ItemManager::CrocPotTable[abs(itemStatsDat->level - itemLooksDat->level)];
    int cost = itemStatsDat->buyPrice * recipe->multStats + itemLooksDat->buyPrice * recipe->multLooks;
    float successChance = recipe->base / 100.0f; // base success chance

    // rarity gap multiplier
    switch (abs(itemStatsDat->rarity - itemLooksDat->rarity)) {
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

void chestOpenHandler(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_CHEST_OPEN *pkt = (sP_CL2FE_REQ_ITEM_CHEST_OPEN *)data->buf;

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
    if (ItemManager::NanoCapsules.find(chest->iID) != ItemManager::NanoCapsules.end())
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

    int itemSetId = -1, rarity = -1, ret = -1;
    bool failing = false;

    // find the crate
    if (ItemManager::Crates.find(chest->iID) == ItemManager::Crates.end()) {
        std::cout << "[WARN] Crate " << chest->iID << " not found!" << std::endl;
        failing = true;
    }
    Crate& crate = ItemManager::Crates[chest->iID];

    if (!failing)
        itemSetId = ItemManager::getItemSetId(crate, chest->iID);
    if (itemSetId == -1)
        failing = true;

    if (!failing)
        rarity = ItemManager::getRarity(crate, itemSetId);
    if (rarity == -1)
        failing = true;

    if (!failing)
        ret = ItemManager::getCrateItem(item->sItem, itemSetId, rarity, plr->PCStyle.iGender);
    if (ret == -1)
        failing = true;

    // if we failed to open a crate, at least give the player a gumball (suggested by Jade)
    if (failing) {
        item->sItem.iType = 7;
        item->sItem.iID = 119 + (rand() % 3);
        item->sItem.iOpt = 1;
    }
    // update player
    plr->Inven[pkt->iSlotNum] = item->sItem;

    // transmit item
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);

    // transmit chest opening acknowledgement packet
    std::cout << "opening chest..." << std::endl;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC));
}

int ItemManager::getItemSetId(Crate& crate, int crateId) {
    int itemSetsCount = crate.itemSets.size();
    if (itemSetsCount == 0) {
        std::cout << "[WARN] Crate " << crateId << " has no item sets assigned?!" << std::endl;
        return -1;
    }

    // if crate points to multiple itemSets, choose a random one
    int itemSetIndex = rand() % itemSetsCount;
    return crate.itemSets[itemSetIndex];
}

int ItemManager::getRarity(Crate& crate, int itemSetId) {
    // find rarity ratio
    if (RarityRatios.find(crate.rarityRatioId) == RarityRatios.end()) {
        std::cout << "[WARN] Rarity Ratio " << crate.rarityRatioId << " not found!" << std::endl;
        return -1;
    }

    std::vector<int> rarityRatio = RarityRatios[crate.rarityRatioId];

    /*
     * First we have to check if specified item set contains items with all specified rarities,
     * and if not eliminate them from the draw
     * it is simpler to do here than to fix individually in the file
     */

    // remember that rarities start from 1!
    for (int i = 0; i < rarityRatio.size(); i++){
        if (CrateItems.find(std::make_pair(itemSetId, i+1)) == CrateItems.end())
            rarityRatio[i] = 0;
    }

    int total = 0;
    for (int value : rarityRatio)
        total += value;

    if (total == 0) {
        std::cout << "Item Set " << itemSetId << " has no items assigned?!" << std::endl;
        return -1;
    }

    // now return a random rarity number
    int randomNum = rand() % total;
    int rarity = 0;
    int sum = 0;
    do {
        sum += rarityRatio[rarity];
        rarity++;
    } while (sum <= randomNum);

    return rarity;
}

int ItemManager::getCrateItem(sItemBase& result, int itemSetId, int rarity, int playerGender) {
    auto key = std::make_pair(itemSetId, rarity);

    if (CrateItems.find(key) == CrateItems.end()) {
        std::cout << "[WARN] Item Set ID " << itemSetId << " Rarity " << rarity << " does not exist" << std::endl;
        return -1;
    }

    // only take into account items that have correct gender
    std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator> items;
    for (auto crateitem : CrateItems[key]) {
        int gender = crateitem->second.gender;
        // if gender is incorrect, exclude item
        if (gender != 0 && gender != playerGender)
            continue;
        items.push_back(crateitem);
    }

    if (items.size() == 0) {
        std::cout << "[WARN] Set ID " << itemSetId << " Rarity " << rarity << " contains no valid items" << std::endl;
        return -1;
    }

    auto item = items[rand() % items.size()];

    result.iID = item->first.first;
    result.iType = item->first.second;
    result.iOpt = 1;

    return 0;
}

// TODO: use this in cleaned up ItemManager
int ItemManager::findFreeSlot(Player *plr) {
    int i;

    for (i = 0; i < AINVEN_COUNT; i++)
        if (plr->Inven[i].iType == 0 && plr->Inven[i].iID == 0 && plr->Inven[i].iOpt == 0)
            return i;

    // not found
    return -1;
}

ItemManager::Item* ItemManager::getItemData(int32_t id, int32_t type) {
    if(ItemData.find(std::make_pair(id, type)) !=  ItemData.end())
        return &ItemData[std::make_pair(id, type)];
    return nullptr;
}

void ItemManager::checkItemExpire(CNSocket* sock, Player* player) {
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
    sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM* packet = (sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM*)respbuf;
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

void ItemManager::setItemStats(Player* plr) {

    plr->pointDamage = 8 + plr->level * 2;
    plr->groupDamage = 8 + plr->level * 2;
    plr->fireRate = 0;
    plr->defense = 16 + plr->level * 4;

    Item* itemStatsDat;

    for (int i = 0; i < 4; i++) {
        itemStatsDat = ItemManager::getItemData(plr->Equip[i].iID, plr->Equip[i].iType);
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
void ItemManager::updateEquips(CNSocket* sock, Player* plr) {
    for (int i = 0; i < 4; i++) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, resp);

        resp.iPC_ID = plr->iID;
        resp.iEquipSlotNum = i;
        resp.EquipSlotItem = plr->Equip[i];

        PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
    }
}

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    // this one is for gumballs
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_USE, itemUseHandler);
    // Bank
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BANK_OPEN, itemBankOpenHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
    // Croc Pot
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_COMBINATION, itemCombineHandler);
}
