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
	
    //weird flip flop but it makes things happen
    resp->eFrom = itemmove->eTo;
    resp->iFromSlotNum = itemmove->iToSlotNum;
    resp->eTo = itemmove->eFrom;
    resp->iToSlotNum = itemmove->iFromSlotNum;
    
    //eFrom 0 means from equip, 1 means from inventory
    if (itemmove->eFrom == 0) {
        resp->FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
    } else {
        resp->FromSlotItem = plr.plr.Inven[itemmove->iFromSlotNum];
    }
    
    //eTo 0 means to equip, 1 means to inventory
    if (itemmove->eTo == 0) {
        resp->ToSlotItem = plr.plr.Equip[itemmove->iToSlotNum];
        plr.plr.Equip[itemmove->iToSlotNum] = resp->FromSlotItem;
    } else {
        resp->ToSlotItem = plr.plr.Inven[itemmove->iToSlotNum];
        plr.plr.Inven[itemmove->iToSlotNum] = resp->FromSlotItem;
    }
    
    if (itemmove->eFrom == 0) {
        plr.plr.Equip[itemmove->iFromSlotNum] = resp->ToSlotItem;
        
        sP_FE2CL_PC_EQUIP_CHANGE* resp2 = (sP_FE2CL_PC_EQUIP_CHANGE*)xmalloc(sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
        
        resp2->iPC_ID = plr.plr.iID;
        resp2->iEquipSlotNum = resp->iToSlotNum;
        resp2->EquipSlotItem = resp->ToSlotItem;
        
        for (CNSocket* otherSock : plr.viewable) {
            otherSock->sendPacket(new CNPacketData((void*)resp2, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE), otherSock->getFEKey()));
        }    
        
    } else {
        plr.plr.Inven[itemmove->iFromSlotNum] = resp->ToSlotItem;
    }
    
    PlayerManager::players[sock] = plr;
    
    sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}