#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "Player.hpp"

#include <string.h> // for memset()
#include <assert.h>

std::map<std::pair<int32_t, int32_t>, Item> ItemManager::ItemData;
std::map<int32_t, std::vector<VendorListing>> ItemManager::VendorTables;
std::map<int32_t, CrocPotEntry> ItemManager::CrocPotTable;
std::map<int32_t, std::vector<int>> ItemManager::RarityRatios;
std::map<int32_t, Crate> ItemManager::Crates;
// pair Itemset, Rarity -> vector of pointers (map iterators) to records in ItemData
std::map<std::pair<int32_t, int32_t>, std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator>> ItemManager::CrateItems;

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);
    // this one is for gumballs
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_USE, itemUseHandler);
    // Bank
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BANK_OPEN, itemBankOpenHandler);
    // Trade handlers
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER, itemTradeOfferHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT, itemTradeOfferAcceptHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL, itemTradeOfferRefusalHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM, itemTradeConfirmHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL, itemTradeConfirmCancelHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_REGISTER, itemTradeRegisterItemHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER, itemTradeUnregisterItemHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CASH_REGISTER, itemTradeRegisterCashHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_EMOTES_CHAT, itemTradeChatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_CHEST_OPEN, chestOpenHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    INITSTRUCT(sP_FE2CL_PC_ITEM_MOVE_SUCC, resp);

    Player* plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (plr->Equip[itemmove->iFromSlotNum].iType != 0 && itemmove->eFrom == 0 && itemmove->eTo == 0) {
        // this packet should never happen unless it is a weapon, tell the client to do nothing and do nothing ourself
        resp.eTo = itemmove->eFrom;
        resp.iToSlotNum = itemmove->iFromSlotNum;
        resp.ToSlotItem = plr->Equip[itemmove->iToSlotNum];
        resp.eFrom = itemmove->eTo;
        resp.iFromSlotNum = itemmove->iToSlotNum;
        resp.FromSlotItem = plr->Equip[itemmove->iFromSlotNum];

        sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
        return;
    }

    // sanity check
    if (itemmove->iToSlotNum > ABANK_COUNT || itemmove->iToSlotNum < 0)
        return;

    // get the fromItem
    sItemBase *fromItem;
    switch ((SlotType)itemmove->eFrom) {
        case SlotType::EQUIP:
            fromItem = &plr->Equip[itemmove->iFromSlotNum];
            break;
        case SlotType::INVENTORY:
            fromItem = &plr->Inven[itemmove->iFromSlotNum];
            break;
        case SlotType::BANK:
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
        toItem = &plr->Equip[itemmove->iToSlotNum];
        break;
    case SlotType::INVENTORY:
        toItem = &plr->Inven[itemmove->iToSlotNum];
        break;
    case SlotType::BANK:
        toItem = &plr->Bank[itemmove->iToSlotNum];
        break;
    default:
        std::cout << "[WARN] MoveItem submitted unknown Item Type?! " << itemmove->eTo << std::endl;
        return;
    }

    // save items to response
    resp.ToSlotItem = *toItem;
    resp.FromSlotItem = *fromItem;

    // swap/stack items in session
    Item* itemDat = getItemData(toItem->iID, toItem->iType);
    if (itemDat->stackSize > 1 && itemDat == getItemData(fromItem->iID, fromItem->iType) && fromItem->iOpt < itemDat->stackSize && toItem->iOpt < itemDat->stackSize) {
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
    resp.eTo = itemmove->eFrom;
    resp.eFrom = itemmove->eTo;


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

    if (player == nullptr || request->iSlotNum < 0 || request->iSlotNum >= AINVEN_COUNT)
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
    time_t until = getTime() + (time_t)NanoManager::SkillTable[144].durationTime[0] * 10;
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

void ItemManager::itemTradeOfferHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER*)data->buf;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
            break;
        }
    }

    Player* plr = PlayerManager::getPlayer(sock);

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

void ItemManager::itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT*)data->buf;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
            break;
        }
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    // Clearing up trade slots

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    if (plr2->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);

        resp.iID_Request = pacdat->iID_To;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));

        return; // prevent trading with a player already trading
    }

    plr->isTrading = true;
    plr2->isTrading = true;
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    memset(&plr->Trade, 0, sizeof(plr->Trade));
    memset(&plr2->Trade, 0, sizeof(plr2->Trade));

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
}

void ItemManager::itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
            break;
        }
    }

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
}

void ItemManager::itemTradeConfirmHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM*)data->buf;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
            break;
        }
    }

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    if (plr2->isTradeConfirm) {

        plr->isTrading = false;
        plr2->isTrading = false;
        plr->isTradeConfirm = false;
        plr2->isTradeConfirm = false;

        // Check if we have enough free slots
        int freeSlots = 0;
        int freeSlotsNeeded = 0;
        int freeSlots2 = 0;
        int freeSlotsNeeded2 = 0;

        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (plr->Inven[i].iID == 0)
                freeSlots++;
        }

        for (int i = 0; i < 5; i++) {
            if (plr->Trade[i].iID != 0)
                freeSlotsNeeded++;
        }

        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (plr2->Inven[i].iID == 0)
                freeSlots2++;
        }

        for (int i = 0; i < 5; i++) {
            if (plr2->Trade[i].iID != 0)
                freeSlotsNeeded2++;
        }

        if (freeSlotsNeeded2 - freeSlotsNeeded > freeSlots) {
            INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);

            resp.iID_Request = pacdat->iID_Request;
            resp.iID_From = pacdat->iID_From;
            resp.iID_To = pacdat->iID_To;

            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
            return; // Fail trade because of the lack of slots
        }

        if (freeSlotsNeeded - freeSlotsNeeded2 > freeSlots2) {
            INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);

            resp.iID_Request = pacdat->iID_Request;
            resp.iID_From = pacdat->iID_From;
            resp.iID_To = pacdat->iID_To;

            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
            return; // Fail trade because of the lack of slots
        }

        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);

        resp.iID_Request = pacdat->iID_Request;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        // ^^ this is a must have or else the player won't accept a succ packet for some reason

        for (int i = 0; i < freeSlotsNeeded; i++) {
            plr->Inven[plr->Trade[i].iInvenNum].iID = 0;
            plr->Inven[plr->Trade[i].iInvenNum].iType = 0;
            plr->Inven[plr->Trade[i].iInvenNum].iOpt = 0;
        }

        for (int i = 0; i < freeSlotsNeeded2; i++) {
            plr2->Inven[plr2->Trade[i].iInvenNum].iID = 0;
            plr2->Inven[plr2->Trade[i].iInvenNum].iType = 0;
            plr2->Inven[plr2->Trade[i].iInvenNum].iOpt = 0;
        }

        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (freeSlotsNeeded <= 0)
                    break;

            if (plr2->Inven[i].iID == 0) {

                plr2->Inven[i].iID = plr->Trade[freeSlotsNeeded - 1].iID;
                plr2->Inven[i].iType = plr->Trade[freeSlotsNeeded - 1].iType;
                plr2->Inven[i].iOpt = plr->Trade[freeSlotsNeeded - 1].iOpt;
                plr->Trade[freeSlotsNeeded - 1].iInvenNum = i;
                freeSlotsNeeded--;
            }
        }

        for (int i = 0; i < AINVEN_COUNT; i++) {
            if (freeSlotsNeeded2 <= 0)
                break;

            if (plr->Inven[i].iID == 0) {

                plr->Inven[i].iID = plr2->Trade[freeSlotsNeeded2 - 1].iID;
                plr->Inven[i].iType = plr2->Trade[freeSlotsNeeded2 - 1].iType;
                plr->Inven[i].iOpt = plr2->Trade[freeSlotsNeeded2 - 1].iOpt;
                plr2->Trade[freeSlotsNeeded2 - 1].iInvenNum = i;
                freeSlotsNeeded2--;
            }
        }

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

        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);

        resp.iID_Request = pacdat->iID_Request;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;

        plr->isTradeConfirm = true;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
    }
}

void ItemManager::itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
        }
    }

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    plr->isTrading = false;
    plr->isTradeConfirm = false;
    plr2->isTrading = false;
    plr2->isTradeConfirm = false;

    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
}

void ItemManager::itemTradeRegisterItemHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER*)data->buf;

    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = pacdat->Item;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
        }
    }

    Player* plr = PlayerManager::getPlayer(sock);
    plr->Trade[pacdat->Item.iSlotNum] = pacdat->Item;
    plr->isTradeConfirm = false;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
}

void ItemManager::itemTradeUnregisterItemHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;

    Player* plr = PlayerManager::getPlayer(sock);
    resp.InvenItem = plr->Trade[pacdat->Item.iSlotNum];
    plr->isTradeConfirm = false;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
        }
    }

    int temp_num = pacdat->Item.iSlotNum;

    if (temp_num >= 0 && temp_num <= 4) {
        plr->Trade[temp_num].iID = 0;
        plr->Trade[temp_num].iType = 0;
        plr->Trade[temp_num].iOpt = 0;
        plr->Trade[temp_num].iInvenNum = 0;
        plr->Trade[temp_num].iSlotNum = 0;
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
}

void ItemManager::itemTradeRegisterCashHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    if (pacdat->iCandy < 0 || pacdat->iCandy > plr->money)
       return; // famous glitch, begone

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.iCandy = pacdat->iCandy;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
        }
    }

    plr->moneyInTrade = pacdat->iCandy;
    plr->isTradeConfirm = false;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
}

void ItemManager::itemTradeChatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT))
        return; // malformed packet
    sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT* pacdat = (sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT*)data->buf;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    memcpy(resp.szFreeChat, pacdat->szFreeChat, sizeof(pacdat->szFreeChat));

    resp.iEmoteCode = pacdat->iEmoteCode;

    int iID_Check;

    if (pacdat->iID_Request == pacdat->iID_From) {
        iID_Check = pacdat->iID_To;
    } else {
        iID_Check = pacdat->iID_From;
    }

    CNSocket* otherSock = sock;

    for (auto pair : PlayerManager::players) {
        if (pair.second->iID == iID_Check) {
            otherSock = pair.first;
        }
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT));
}

void ItemManager::chestOpenHandler(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_CHEST_OPEN *chest = (sP_CL2FE_REQ_ITEM_CHEST_OPEN *)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // chest opening acknowledgement packet
    INITSTRUCT(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC, resp);
    resp.iSlotNum = chest->iSlotNum;

    if (plr == nullptr)
        return;

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

    item->iSlotNum = chest->iSlotNum;
    item->eIL = chest->eIL;

    // item reward
    if (chest->ChestItem.iType != 9) {
        std::cout << "[WARN] Player tried to open a crate with incorrect iType ?!" << std::endl;
        return;
    }

    int itemSetId = -1, rarity = -1, ret = -1;
    bool failing = false;

    // find the crate
    if (Crates.find(chest->ChestItem.iID) == Crates.end()) {
        std::cout << "[WARN] Crate " << chest->ChestItem.iID << " not found!" << std::endl;
        failing = true;
    }
    Crate& crate = Crates[chest->ChestItem.iID];

    if (!failing)
        itemSetId = getItemSetId(crate, chest->ChestItem.iID);
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
    plr->Inven[chest->iSlotNum] = item->sItem;

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
        std::cout << "[WARN] Item Set ID " << itemSetId << " Rarity " << rarity << " items have not been found" << std::endl;
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
        std::cout << "[WARN] Gender inequality! Set ID " << itemSetId << " Rarity " << rarity << " contains only "
                  << (playerGender == 2 ? "boys" : "girls") << " items?!" << std::endl;
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

Item* ItemManager::getItemData(int32_t id, int32_t type) {
    if(ItemData.find(std::pair<int32_t, int32_t>(id, type)) !=  ItemData.end())
        return &ItemData[std::pair<int32_t, int32_t>(id, type)];
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
