#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ITEM_DELETE, itemDeleteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    sP_FE2CL_PC_ITEM_MOVE_SUCC* resp = (sP_FE2CL_PC_ITEM_MOVE_SUCC*)xmalloc(sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));

    PlayerView& plr = PlayerManager::players[sock];
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
        for (CNSocket* otherSock : plr.viewable) {
            sP_FE2CL_PC_EQUIP_CHANGE* resp2 = (sP_FE2CL_PC_EQUIP_CHANGE*)xmalloc(sizeof(sP_FE2CL_PC_EQUIP_CHANGE));

            resp2->iPC_ID = plr.plr.iID;
            if (itemmove->eFrom == 0) {
                resp2->iEquipSlotNum = itemmove->iFromSlotNum;
                resp2->EquipSlotItem = toItem;
            } else {
                resp2->iEquipSlotNum = itemmove->iToSlotNum;
                resp2->EquipSlotItem = fromItem;
            }
            otherSock->sendPacket(new CNPacketData((void*)resp2, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE), otherSock->getFEKey()));
        }
    }

    resp->eTo = itemmove->eFrom;
    resp->iToSlotNum = itemmove->iFromSlotNum;
    resp->ToSlotItem = toItem;
    resp->eFrom = itemmove->eTo;
    resp->iFromSlotNum = itemmove->iToSlotNum;
    resp->FromSlotItem = fromItem;

    sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}

void ItemManager::itemDeleteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ITEM_DELETE* itemdel = (sP_CL2FE_REQ_PC_ITEM_DELETE*)data->buf;
    sP_FE2CL_REP_PC_ITEM_DELETE_SUCC* resp = (sP_FE2CL_REP_PC_ITEM_DELETE_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC));

    PlayerView& plr = PlayerManager::players[sock];

    resp->eIL = itemdel->eIL;
    resp->iSlotNum = itemdel->iSlotNum;

    // so, im not sure what this eIL thing does since you always delete items in inventory and not equips
    plr.plr.Inven[itemdel->iSlotNum].iID = 0;
    plr.plr.Inven[itemdel->iSlotNum].iType = 0;
    plr.plr.Inven[itemdel->iSlotNum].iOpt = 0;

    sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_ITEM_DELETE_SUCC, sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC), sock->getFEKey()));
}

void ItemManager::itemGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GIVE_ITEM* itemreq = (sP_CL2FE_REQ_PC_GIVE_ITEM*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    if (!plr.plr.IsGM) {
    	// TODO: send fail packet
        return;
    }
	
    if (itemreq->eIL == 2) {
        // Quest item, not a real item, handle this later, stubbed for now
        // sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_FAIL, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL), sock->getFEKey()));
    } else if (itemreq->eIL == 1) {
        sP_FE2CL_REP_PC_GIVE_ITEM_SUCC* resp = (sP_FE2CL_REP_PC_GIVE_ITEM_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));

        resp->eIL = itemreq->eIL;
        resp->iSlotNum = itemreq->iSlotNum;
        resp->Item = itemreq->Item;

        plr.plr.Inven[itemreq->iSlotNum] = itemreq->Item;
        plr.plr.level = 36;

        sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC), sock->getFEKey()));

        sP_FE2CL_REP_PC_CHANGE_LEVEL* resp2 = (sP_FE2CL_REP_PC_CHANGE_LEVEL*)xmalloc(sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));

        resp2->iPC_ID = plr.plr.iID;
        resp2->iPC_Level = 36;

        sock->sendPacket(new CNPacketData((void*)resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL), sock->getFEKey()));
    }
}