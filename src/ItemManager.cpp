#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER, itemTradeOfferHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT, itemTradeOfferAcceptHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL, itemTradeOfferRefusalHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL, itemTradeConfirmCancelHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    INITSTRUCT(sP_FE2CL_PC_ITEM_MOVE_SUCC, resp);
    
    PlayerView& plr = PlayerManager::players[sock];
    
    if (itemmove->eFrom == 0 && itemmove->eTo == 0) {
        // this packet should never happen, tell the client to do nothing and do nothing ourself
        resp.eTo = itemmove->eFrom;
        resp.iToSlotNum = itemmove->iFromSlotNum;
        resp.ToSlotItem = plr.plr.Equip[itemmove->iToSlotNum];
        resp.eFrom = itemmove->eTo;
        resp.iFromSlotNum = itemmove->iToSlotNum;
        resp.FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
        
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));
        return;
    }
    
    sItemBase fromItem;
    sItemBase toItem;

    // eFrom 0 means from equip
    if (itemmove->eFrom == 0) {
        // unequiping an item
        fromItem = plr.plr.Equip[itemmove->iFromSlotNum];
    } else {
        fromItem = plr.plr.Inven[itemmove->iFromSlotNum];
    }

    // eTo 0 means to equip
    if (itemmove->eTo == 0) {
        // equiping an item
        toItem = plr.plr.Equip[itemmove->iToSlotNum];
        plr.plr.Equip[itemmove->iToSlotNum] = fromItem;
    } else {
        toItem = plr.plr.Inven[itemmove->iToSlotNum];
        plr.plr.Inven[itemmove->iToSlotNum] = fromItem;
    }

    if (itemmove->eFrom == 0) {
        plr.plr.Equip[itemmove->iFromSlotNum] = toItem;
    } else {
        plr.plr.Inven[itemmove->iFromSlotNum] = toItem;
    }

    if (itemmove->eFrom == 0 || itemmove->eTo == 0) {
        INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, equipChange);

        equipChange.iPC_ID = plr.plr.iID;
        if (itemmove->eFrom == 0) {
            equipChange.iEquipSlotNum = itemmove->iFromSlotNum;
            equipChange.EquipSlotItem = toItem;
        } else {
            equipChange.iEquipSlotNum = itemmove->iToSlotNum;
            equipChange.EquipSlotItem = fromItem;
        }

        // send equip event to other players
        for (CNSocket* otherSock : plr.viewable) {
            otherSock->sendPacket((void*)&equipChange, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
        }
    }

    resp.eTo = itemmove->eFrom;
    resp.iToSlotNum = itemmove->iFromSlotNum;
    resp.ToSlotItem = toItem;
    resp.eFrom = itemmove->eTo;
    resp.iFromSlotNum = itemmove->iToSlotNum;
    resp.FromSlotItem = fromItem;

    sock->sendPacket((void*)&resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
}

void ItemManager::itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ITEM_DELETE* itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC, resp);

    PlayerView& plr = PlayerManager::players[sock];

    resp.eIL = itemdel->eIL;
    resp.iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr.plr.Inven[itemdel->iSlotNum].iID = 0;
    plr.plr.Inven[itemdel->iSlotNum].iType = 0;
    plr.plr.Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));
}

void ItemManager::itemGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GIVE_ITEM* itemreq = (sP_CL2FE_REQ_PC_GIVE_ITEM*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    // Commented and disabled for future use
    //if (!plr.plr.IsGM) {
    	// TODO: send fail packet
    //    return;
    //}

    if (itemreq->eIL == 2) {
        // Quest item, not a real item, handle this later, stubbed for now
        // sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_FAIL, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL), sock->getFEKey()));
    } else if (itemreq->eIL == 1 && itemreq->Item.iType >= 0 && itemreq->Item.iType <= 8) {
        
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = itemreq->eIL;
        resp.iSlotNum = itemreq->iSlotNum;
        resp.Item = itemreq->Item;

        plr.plr.Inven[itemreq->iSlotNum] = itemreq->Item;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));

        // some items require a level, for now we're just going to bypass this by setting your level to 36
        //plr.plr.level = 36;
        
        //sP_FE2CL_REP_PC_CHANGE_LEVEL resp2;

        //resp2.iPC_ID = plr.plr.iID;
        //resp2.iPC_Level = 36;

        //sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
        // saving this for later use on a /level command
    }
}

void ItemManager::itemTradeOfferHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);
    
    DEBUGLOG(
                std::cout << "\tiID_Request: " << (int)pacdat->iID_Request << std::endl;
                std::cout << "\tiID_From: " << (int)pacdat->iID_From << std::endl;
                std::cout << "\tiID_To: " << (int)pacdat->iID_To << std::endl;
            )

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_To;
    resp.iID_To = pacdat->iID_From;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER));
}

void ItemManager::itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_To;
    resp.iID_To = pacdat->iID_From;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
}

void ItemManager::itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_To;
    resp.iID_To = pacdat->iID_From;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
}

void ItemManager::itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL))
        return; // ignore the malformed packet

    sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL* pacdat = (sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);

    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_To;
    resp.iID_To = pacdat->iID_From;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
}