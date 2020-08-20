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
    
    sItemBase temp;
    
    if (itemmove->eFrom < 1) {
        if (itemmove->eTo < 1) {
            //equip to equip, if this happens, something very wrong has happened
            temp = plr.plr.Equip[itemmove->iToSlotNum];
            plr.plr.Equip[itemmove->iToSlotNum] = plr.plr.Equip[itemmove->iFromSlotNum];
            plr.plr.Equip[itemmove->iFromSlotNum] = temp;
            resp->FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
            resp->ToSlotItem = plr.plr.Equip[itemmove->iToSlotNum];
        } else {
            //equip to inventory
            temp = plr.plr.Inven[itemmove->iToSlotNum];
            plr.plr.Inven[itemmove->iToSlotNum] = plr.plr.Equip[itemmove->iFromSlotNum];
            plr.plr.Equip[itemmove->iFromSlotNum] = temp;
            resp->FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
            resp->ToSlotItem = plr.plr.Inven[itemmove->iToSlotNum];
        }
    } else {
        if (itemmove->eTo < 1) {
            //inventory to equip
            temp = plr.plr.Equip[itemmove->iToSlotNum];
            plr.plr.Equip[itemmove->iToSlotNum] = plr.plr.Inven[itemmove->iFromSlotNum];
            plr.plr.Inven[itemmove->iFromSlotNum] = temp;
            resp->FromSlotItem = plr.plr.Equip[itemmove->iFromSlotNum];
            resp->ToSlotItem = plr.plr.Inven[itemmove->iToSlotNum];       
        } else {
           //inventory to inventory
            temp = plr.plr.Inven[itemmove->iToSlotNum];
            plr.plr.Inven[itemmove->iToSlotNum] = plr.plr.Inven[itemmove->iFromSlotNum];
            plr.plr.Inven[itemmove->iFromSlotNum] = temp;
            resp->FromSlotItem = plr.plr.Inven[itemmove->iFromSlotNum];
            resp->ToSlotItem = plr.plr.Inven[itemmove->iToSlotNum]; 
        }
    }
    
    DEBUGLOG(
        std::cout << "Sent Data:" << std::endl;
        std::cout << "\tFrom: " << resp->FromSlotItem.iID << std::endl;
        std::cout << "\tTo: " << resp->ToSlotItem.iID << std::endl;
    )
    
    for (int i = 0; i < AEQUIP_COUNT; i++) {
        DEBUGLOG(
            std::cout <<plr.plr.Equip[i].iID << std::endl;  
        )
    }
    
    for (int i = 0; i < AINVEN_COUNT; i++) {
        DEBUGLOG(
            std::cout <<plr.plr.Inven[i].iID << std::endl;  
        )                   
    }
    
	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_PC_ITEM_MOVE_SUCC, sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC), sock->getFEKey()));
}