#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
    REGISTER_SHARD_PACKET(P_FE2CL_PC_EQUIP_CHANGE, itemMoveHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
    sP_FE2CL_PC_ITEM_MOVE_SUCC* resp = (sP_FE2CL_PC_ITEM_MOVE_SUCC*)xmalloc(sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));

    PlayerView plr = PlayerManager::players[sock];
    sItemBase fromItem;
    sItemBase toItem;
    
    // eFrom 0 means from equip
    if (itemmove->eFrom == 0) {
        // unequiping an item
        std::cout << "unequipting item" << std::endl;
        fromItem = plr.plr.Equip[itemmove->iFromSlotNum];
    } else {
        fromItem = plr.plr.Inven[itemmove->iFromSlotNum];
    }
    
    // eTo 0 means to equip
    if (itemmove->eTo == 0) {
        std::cout << "equipting item" << std::endl;
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
            }
            else {
                resp2->iEquipSlotNum = itemmove->iToSlotNum;
                resp2->EquipSlotItem = fromItem;
            }
            otherSock->sendPacket(new CNPacketData((void*)resp2, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE), otherSock->getFEKey()));
        }   
    }

    PlayerManager::players[sock] = plr;
    
    resp->eTo = itemmove->eFrom;
    resp->iToSlotNum = itemmove->iFromSlotNum;
    resp->ToSlotItem = toItem;
    resp->eFrom = itemmove->eTo;
    resp->iFromSlotNum = itemmove->iToSlotNum;
    resp->FromSlotItem = fromItem;

    sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}