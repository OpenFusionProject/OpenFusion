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
	
	resp->eFrom = itemmove->eFrom;
	resp->iFromSlotNum = itemmove->iFromSlotNum;
	resp->FromSlotItem = plr.plr.Equip[1];
	resp->eTo = itemmove->eTo;
	resp->iToSlotNum = itemmove->iToSlotNum;
	resp->ToSlotItem = plr.plr.Equip[1];
    
	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}