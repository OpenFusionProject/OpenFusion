#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ItemManager.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"

void ItemManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ITEM_MOVE, itemMoveHandler);
}

void ItemManager::itemMoveHandler(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_ITEM_MOVE))
        return; // ignore the malorn packet
	
	sP_CL2FE_REQ_ITEM_MOVE* itemmove = (sP_CL2FE_REQ_ITEM_MOVE*)data->buf;
	
	PlayerView plr = PlayerManager::players[sock];
	
	sP_FE2CL_PC_ITEM_MOVE_SUCC* resp = (sP_FE2CL_PC_ITEM_MOVE_SUCC*)xmalloc(sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC));
    
    DEBUGLOG(
        std::cout << "Item Move Received:" << std::endl;
        std::cout << "\teFrom: " << itemmove->eFrom << std::endl;
        std::cout << "\tiFromSlotNum: " << itemmove->iFromSlotNum << std::endl;
        std::cout << "\teTo: " << itemmove->eTo << std::endl;
        std::cout << "\tiToSlotNum: " << itemmove->iToSlotNum << std::endl;
    )
	
	resp->eFrom = itemmove->eFrom;
	resp->iFromSlotNum = itemmove->iFromSlotNum;
	resp->eTo = itemmove->eTo;
	resp->iToSlotNum = itemmove->iToSlotNum;
    
    //please don't reee at me
    if (itemmove->eFrom < 1) {
        resp->FromSlotItem = plr.plr.Inven[itemmove->iFromSlotNum];
    } else {
        resp->FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
    }
    
    if (itemmove->eTo > 0) {
        resp->ToSlotItem = plr.plr.Inven[itemmove->iToSlotNum];
    } else {
        resp->ToSlotItem = plr.plr.Equip[itemmove->iToSlotNum];
    }

    
	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}