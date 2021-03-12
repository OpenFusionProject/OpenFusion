#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "Player.hpp"

#include <string.h> // for memset()
#include <assert.h>

std::map<std::pair<int32_t, int32_t>, ItemManager::Item> ItemManager::ItemData;
std::map<int32_t, std::vector<VendorListing>> ItemManager::VendorTables;
std::map<int32_t, CrocPotEntry> ItemManager::CrocPotTable;
std::map<int32_t, std::vector<int>> ItemManager::RarityRatios;
std::map<int32_t, Crate> ItemManager::Crates;
// pair Itemset, Rarity -> vector of pointers (map iterators) to records in ItemData
std::map<std::pair<int32_t, int32_t>, std::vector<std::map<std::pair<int32_t, int32_t>, ItemManager::Item>::iterator>> ItemManager::CrateItems;
std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> ItemManager::CodeItems;

#ifdef ACADEMY
std::map<int32_t, int32_t> ItemManager::NanoCapsules; // crate id -> nano id
#endif

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);
    // this one is for gumballs
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_USE, itemUseHandler);
    // Bank
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BANK_OPEN, itemBankOpenHandler);
    // Trade handlers
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER, tradeOffer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT, tradeOfferAccept);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL, tradeOfferRefusal);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM, tradeConfirm);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL, tradeConfirmCancel);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_REGISTER, tradeRegisterItem);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER, tradeUnregisterItem);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CASH_REGISTER, tradeRegisterCash);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
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
        if (plr->Equip[8].iID == 0)
            plr->iPCState = 0;

        // send equip event to other players
        PlayerManager::sendToViewable(sock, (void*)&equipChange, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));

        // set equipment stats serverside
        setItemStats(plr);
    }

    // send response
    sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
}

void ItemManager::itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
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

void ItemManager::itemGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GIVE_ITEM* itemreq = (sP_CL2FE_REQ_PC_GIVE_ITEM*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 50) {
    	// TODO: send fail packet
        return;
    }

    if (itemreq->eIL == 2) {
        // Quest item, not a real item, handle this later, stubbed for now
        // sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_FAIL, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL), sock->getFEKey()));
    } else if (itemreq->eIL == 1 && itemreq->Item.iType >= 0 && itemreq->Item.iType <= 10) {

        if (ItemData.find(std::pair<int32_t, int32_t>(itemreq->Item.iID, itemreq->Item.iType)) == ItemData.end()) {
            // invalid item
            std::cout << "[WARN] Item id " << itemreq->Item.iID << " with type " << itemreq->Item.iType << " is invalid (give item)" << std::endl;
            return;
        }

        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = itemreq->eIL;
        resp.iSlotNum = itemreq->iSlotNum;
        if (itemreq->Item.iType == 10) {
            // item is vehicle, set expiration date
            // set time limit: current time + 7days
            itemreq->Item.iTimeLimit = getTimestamp() + 604800;
        }
        resp.Item = itemreq->Item;

        plr->Inven[itemreq->iSlotNum] = itemreq->Item;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
    }
}

void ItemManager::itemUseHandler(CNSocket* sock, CNPacketData* data) {
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

void ItemManager::itemBankOpenHandler(CNSocket* sock, CNPacketData* data) {
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

void ItemManager::tradeOffer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_To);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(otherSock);

    if (plr->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
        resp.iID_Request = pacdat->iID_To;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        return; // prevent trading with a player already trading
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER));
}

void ItemManager::tradeOfferAccept(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    if (plr2->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
        resp.iID_Request = pacdat->iID_From;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        return; // prevent trading with a player already trading
    }

    // clearing up trade slots
    plr->moneyInTrade = 0;
    plr2->moneyInTrade = 0;
    memset(&plr->Trade, 0, sizeof(plr->Trade));
    memset(&plr2->Trade, 0, sizeof(plr2->Trade));

    // marking players as traders
    plr->isTrading = true;
    plr2->isTrading = true;

    // marking players as unconfirmed
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    // inform the other player that offer is accepted
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
}

void ItemManager::tradeOfferRefusal(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
}

void ItemManager::tradeConfirm(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM*)data->buf;

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);
    
    if (!(plr->isTrading && plr2->isTrading)) { // both players must be trading
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);
        resp.iID_Request = plr2->iID;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        resp.iID_Request = plr->iID;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        return;
    }

    // send the confirm packet
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));

    if (!(plr2->isTradeConfirm)) {
        plr->isTradeConfirm = true;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        return;
    }

    // both players are no longer trading
    plr->isTrading = false;
    plr2->isTrading = false;
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    if (doTrade(plr, plr2)) { // returns false if not enough slots
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, resp2);
        resp2.iID_Request = pacdat->iID_Request;
        resp2.iID_From = pacdat->iID_From;
        resp2.iID_To = pacdat->iID_To;
        plr->money = plr->money + plr2->moneyInTrade - plr->moneyInTrade;
        resp2.iCandy = plr->money;
        memcpy(resp2.Item, plr2->Trade, sizeof(plr2->Trade));
        memcpy(resp2.ItemStay, plr->Trade, sizeof(plr->Trade));

        sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));

        plr2->money = plr2->money + plr->moneyInTrade - plr2->moneyInTrade;
        resp2.iCandy = plr2->money;
        memcpy(resp2.Item, plr->Trade, sizeof(plr->Trade));
        memcpy(resp2.ItemStay, plr2->Trade, sizeof(plr2->Trade));

        otherSock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));
    } else {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);
        resp.iID_Request = plr2->iID;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        resp.iID_Request = plr->iID;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        return;
    }
}

bool ItemManager::doTrade(Player* plr, Player* plr2) {
    // init dummy inventories
    sItemBase plrInven[AINVEN_COUNT];
    sItemBase plr2Inven[AINVEN_COUNT];
    memcpy(plrInven, plr->Inven, AINVEN_COUNT * sizeof(sItemBase));
    memcpy(plr2Inven, plr2->Inven, AINVEN_COUNT * sizeof(sItemBase));

    for (int i = 0; i < 5; i++) {
        // remove items offered by us
        if (plr->Trade[i].iID != 0) {
            if (plrInven[plr->Trade[i].iInvenNum].iID == 0
                || plr->Trade[i].iID != plrInven[plr->Trade[i].iInvenNum].iID
                || plr->Trade[i].iType != plrInven[plr->Trade[i].iInvenNum].iType) // pulling a fast one on us
                return false;

            if (plr->Trade[i].iOpt < 1) {
                std::cout << "[WARN] Player tried trading an iOpt < 1 amount" << std::endl;
                plr->Trade[i].iOpt = 1;
            }

            // for stacked items
            plrInven[plr->Trade[i].iInvenNum].iOpt -= plr->Trade[i].iOpt;

            if (plrInven[plr->Trade[i].iInvenNum].iOpt == 0) {
                plrInven[plr->Trade[i].iInvenNum].iID = 0;
                plrInven[plr->Trade[i].iInvenNum].iType = 0;
                plrInven[plr->Trade[i].iInvenNum].iOpt = 0;
            } else if (plrInven[plr->Trade[i].iInvenNum].iOpt < 0) { // another dupe attempt
                return false;
            }
        }

        if (plr2->Trade[i].iID != 0) {
            if (plr2Inven[plr2->Trade[i].iInvenNum].iID == 0
                || plr2->Trade[i].iID != plr2Inven[plr2->Trade[i].iInvenNum].iID
                || plr2->Trade[i].iType != plr2Inven[plr2->Trade[i].iInvenNum].iType) // pulling a fast one on us
                return false;

            if (plr2->Trade[i].iOpt < 1) {
                std::cout << "[WARN] Player tried trading an iOpt < 1 amount" << std::endl;
                plr2->Trade[i].iOpt = 1;
            }

            // for stacked items
            plr2Inven[plr2->Trade[i].iInvenNum].iOpt -= plr2->Trade[i].iOpt;

            if (plr2Inven[plr2->Trade[i].iInvenNum].iOpt == 0) {
                plr2Inven[plr2->Trade[i].iInvenNum].iID = 0;
                plr2Inven[plr2->Trade[i].iInvenNum].iType = 0;
                plr2Inven[plr2->Trade[i].iInvenNum].iOpt = 0;
            } else if (plr2Inven[plr2->Trade[i].iInvenNum].iOpt < 0) { // another dupe attempt
                return false;
            }
        }

        // add items offered to us
        if (plr2->Trade[i].iID != 0) {
            for (int n = 0; n < AINVEN_COUNT; n++) {
                if (plrInven[n].iID == 0) {
                    plrInven[n].iID = plr2->Trade[i].iID;
                    plrInven[n].iType = plr2->Trade[i].iType;
                    plrInven[n].iOpt = plr2->Trade[i].iOpt;
                    plr2->Trade[i].iInvenNum = n;
                    break;
                }

                if (n >= AINVEN_COUNT - 1)
                    return false; // not enough space
            }
        }

        if (plr->Trade[i].iID != 0) {
            for (int n = 0; n < AINVEN_COUNT; n++) {
                if (plr2Inven[n].iID == 0) {
                    plr2Inven[n].iID = plr->Trade[i].iID;
                    plr2Inven[n].iType = plr->Trade[i].iType;
                    plr2Inven[n].iOpt = plr->Trade[i].iOpt;
                    plr->Trade[i].iInvenNum = n;
                    break;
                }

                if (n >= AINVEN_COUNT - 1)
                    return false; // not enough space
            }
        }
    }

    // if everything went well, back into player inventory it goes
    memcpy(plr->Inven, plrInven, AINVEN_COUNT * sizeof(sItemBase));
    memcpy(plr2->Inven, plr2Inven, AINVEN_COUNT * sizeof(sItemBase));

    return true;
}

void ItemManager::tradeConfirmCancel(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL*)data->buf;

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    // both players are not trading nor are in a confirmed state
    plr->isTrading = false;
    plr->isTradeConfirm = false;
    plr2->isTrading = false;
    plr2->isTradeConfirm = false;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
}

void ItemManager::tradeRegisterItem(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER*)data->buf;

    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check, there are only 5 trade slots

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    plr->Trade[pacdat->Item.iSlotNum] = pacdat->Item;
    plr->isTradeConfirm = false;

    // since you can spread items like gumballs over multiple slots, we need to count them all
    // to make sure the inventory shows the right value during trade.
    int count = 0; 
    for (int i = 0; i < 5; i++) {
        if (plr->Trade[i].iInvenNum == pacdat->Item.iInvenNum)
            count += plr->Trade[i].iOpt;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = pacdat->Item;
    resp.InvenItem.iOpt = plr->Inven[pacdat->Item.iInvenNum].iOpt - count; // subtract this count

    if (resp.InvenItem.iOpt < 0) // negative count items, doTrade() will block this later on
        std::cout << "[WARN] tradeRegisterItem: an item went negative count client side." << std::endl;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
}

void ItemManager::tradeUnregisterItem(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER*)data->buf;

    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check, there are only 5 trade slots

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    plr->isTradeConfirm = false;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = plr->Trade[pacdat->Item.iSlotNum];

    memset(&plr->Trade[pacdat->Item.iSlotNum], 0, sizeof(plr->Trade[pacdat->Item.iSlotNum])); // clean up item slot

    // since you can spread items like gumballs over multiple slots, we need to count them all
    // to make sure the inventory shows the right value during trade.
    int count = 0; 
    for (int i = 0; i < 5; i++) {
        if (plr->Trade[i].iInvenNum == resp.InvenItem.iInvenNum)
            count += plr->Trade[i].iOpt;
    }

    resp.InvenItem.iOpt = plr->Inven[resp.InvenItem.iInvenNum].iOpt - count; // subtract this count

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
}

void ItemManager::tradeRegisterCash(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    if (pacdat->iCandy < 0 || pacdat->iCandy > plr->money)
       return; // famous glitch, begone

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.iCandy = pacdat->iCandy;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));

    plr->moneyInTrade = pacdat->iCandy;
    plr->isTradeConfirm = false;
}

void ItemManager::chestOpenHandler(CNSocket *sock, CNPacketData *data) {
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

    int itemSetId = -1, rarity = -1, ret = -1;
    bool failing = false;

    // find the crate
    if (Crates.find(chest->iID) == Crates.end()) {
        std::cout << "[WARN] Crate " << chest->iID << " not found!" << std::endl;
        failing = true;
    }
    Crate& crate = Crates[chest->iID];

    if (!failing)
        itemSetId = getItemSetId(crate, chest->iID);
    if (itemSetId == -1)
        failing = true;

    if (!failing)
        rarity = getRarity(crate, itemSetId);
    if (rarity == -1)
        failing = true;

    if (!failing)
        ret = getCrateItem(item->sItem, itemSetId, rarity, plr->PCStyle.iGender);
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
    std::pair key = std::make_pair(itemSetId, rarity);

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

#ifdef ACADEMY
void ItemManager::nanoCapsuleHandler(CNSocket* sock, int slot, sItemBase *chest) {
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
