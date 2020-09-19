#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "BuddyManager.hpp"

#include <algorithm>

void BuddyManager::init() {
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_REQUEST_MAKE_BUDDY, requestBuddy);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY, reqBuddyByName);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_ACCEPT_MAKE_BUDDY, reqAcceptBuddy);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY, reqFindNameBuddyAccept);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE, reqBuddyFreechat);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE, reqBuddyMenuchat);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_GET_BUDDY_STATE, reqPktGetBuddyState);
}

//Buddy request
void BuddyManager::requestBuddy(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_REQUEST_MAKE_BUDDY))
		return; // malformed packet

	Player* plrReq = PlayerManager::getPlayer(sock);

	sP_CL2FE_REQ_REQUEST_MAKE_BUDDY* pkt = (sP_CL2FE_REQ_REQUEST_MAKE_BUDDY*)data->buf;

	INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC, resp);

	CNSocket* otherSock = sock;

	for (auto pair : PlayerManager::players) {
		if (pair.second.plr->PCStyle.iPC_UID == pkt->iBuddyPCUID) {
			otherSock = pair.first;
		}
	}

	PlayerView& plr = PlayerManager::players[otherSock];

	resp.iRequestID = plr.plr->iID;
	resp.iBuddyID = plr.plr->iID;
	resp.iBuddyPCUID = plr.plr->PCStyle.iPC_UID;

	sock->sendPacket((void*)&resp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC));
	requestedBuddy(otherSock, plrReq, plr);

}

//Sending buddy request by player name
void BuddyManager::reqBuddyByName(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY)) {
		return; // malformed packet
	}

	sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY* pkt = (sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY*)data->buf;
	Player* plrReq = PlayerManager::getPlayer(sock);

	INITSTRUCT(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC, resp);

	CNSocket* otherSock = sock;

	int sizeOfRes = sizeof(pkt->szFirstName) / 9;
	int sizeOfLNRes = sizeof(pkt->szLastName) / 17;

	for (auto pair : PlayerManager::players) {
		int sizeOfReq = sizeof(pair.second.plr->PCStyle.szFirstName) / 9;
		int sizeOfLNReq = sizeof(pair.second.plr->PCStyle.szLastName) / 17;
		if (BuddyManager::firstNameCheck(pair.second.plr->PCStyle.szFirstName, pkt->szFirstName, sizeOfReq, sizeOfRes) == true && BuddyManager::lastNameCheck(pair.second.plr->PCStyle.szLastName, pkt->szLastName, sizeOfLNReq, sizeOfLNRes) == true) {
			otherSock = pair.first;
		}
	}

	PlayerView& plr = PlayerManager::players[otherSock];

	resp.iPCUID = plrReq->PCStyle.iPC_UID;
	resp.iNameCheckFlag = plrReq->PCStyle.iNameCheck;

	for (int fName = 0; fName < 9; fName++) {
		resp.szFirstName[fName] = plrReq->PCStyle.szFirstName[fName];
	}

	for (int lName = 0; lName < 17; lName++) {
		resp.szLastName[lName] = plrReq->PCStyle.szLastName[lName];
	}

	otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC));

}

//Accepting buddy request
void BuddyManager::reqAcceptBuddy(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY))
		return; // malformed packet

	sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY* pkt = (sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY*)data->buf;
	Player* plrReq = PlayerManager::getPlayer(sock);

	INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

	CNSocket* otherSock = sock;

	for (auto pair : PlayerManager::players) {
		if (pair.second.plr->PCStyle.iPC_UID == pkt->iBuddyPCUID) {
			otherSock = pair.first;
		}
	}

	PlayerView& plr = PlayerManager::players[otherSock];

	if (pkt->iAcceptFlag == 1) {
		//resp.iBuddySlot = 0; //hard-coding this for now
		resp.BuddyInfo.iID = pkt->iBuddyID;
		resp.BuddyInfo.iPCUID = pkt->iBuddyPCUID;
		resp.BuddyInfo.iNameCheckFlag = plr.plr->PCStyle.iNameCheck;
		resp.BuddyInfo.iPCState = plr.plr->iPCState;
		resp.BuddyInfo.iGender = plr.plr->PCStyle.iGender;
		resp.BuddyInfo.bBlocked = 0;
		resp.BuddyInfo.bFreeChat = 1;

		for (int i = 0; i < 9; i++) {
			resp.BuddyInfo.szFirstName[i] = plr.plr->PCStyle.szFirstName[i];
		}

		for (int i = 0; i < 17; i++) {
			resp.BuddyInfo.szLastName[i] = plr.plr->PCStyle.szLastName[i];
		}

		sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
		buddyList(sock, resp.BuddyInfo);
		otherAcceptBuddy(otherSock, pkt->iBuddyID, pkt->iBuddyPCUID, resp, plrReq);
	}
	else {
		INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

		declineResp.iErrorCode = 6; //Buddy declined notification
		declineResp.iBuddyID = pkt->iBuddyID;
		declineResp.iBuddyPCUID = pkt->iBuddyPCUID;

		otherSock->sendPacket((void*)&declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL));
	}

	
}

//Accepting buddy request from the find name request
void BuddyManager::reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY)) {
		return; // malformed packet
	}

	sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY* pkt = (sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY*)data->buf;
	Player* plrReq = PlayerManager::getPlayer(sock);

	INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

	CNSocket* otherSock = sock;

	int sizeOfRes = sizeof(pkt->szFirstName) / 9;
	int sizeOfLNRes = sizeof(pkt->szLastName) / 17;

	for (auto pair : PlayerManager::players) {
		int sizeOfReq = sizeof(pair.second.plr->PCStyle.szFirstName) / 9;
		int sizeOfLNReq = sizeof(pair.second.plr->PCStyle.szLastName) / 17;
		if (BuddyManager::firstNameCheck(pair.second.plr->PCStyle.szFirstName, pkt->szFirstName, sizeOfReq, sizeOfRes) == true && BuddyManager::lastNameCheck(pair.second.plr->PCStyle.szLastName, pkt->szLastName, sizeOfLNReq, sizeOfLNRes) == true) {
			otherSock = pair.first;
		}
	}

	PlayerView& plr = PlayerManager::players[otherSock];

	if (pkt->iAcceptFlag == 1) {
		//resp.iBuddySlot = 0; //hard-coding this for now
		resp.BuddyInfo.iID = plr.plr->iID;
		resp.BuddyInfo.iPCUID = pkt->iBuddyPCUID;
		resp.BuddyInfo.iNameCheckFlag = plr.plr->PCStyle.iNameCheck;
		resp.BuddyInfo.iPCState = plr.plr->iPCState;
		resp.BuddyInfo.iGender = plr.plr->PCStyle.iGender;
		resp.BuddyInfo.bBlocked = 0;
		resp.BuddyInfo.bFreeChat = 1;

		for (int i = 0; i < 9; i++) {
			resp.BuddyInfo.szFirstName[i] = plr.plr->PCStyle.szFirstName[i];
		}

		for (int i = 0; i < 17; i++) {
			resp.BuddyInfo.szLastName[i] = plr.plr->PCStyle.szLastName[i];
		}

		sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
		buddyList(sock, resp.BuddyInfo);
		otherAcceptBuddy(otherSock, plr.plr->iID, pkt->iBuddyPCUID, resp, plrReq);
	}
	else {
		INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

		declineResp.iErrorCode = 6; //Buddy declined notification
		declineResp.iBuddyID = plr.plr->iID;
		declineResp.iBuddyPCUID = pkt->iBuddyPCUID;

		otherSock->sendPacket((void*)&declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL));
	}

}

//Buddy freechatting
void BuddyManager::reqBuddyFreechat(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE))
		return; // malformed packet

	sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE* pkt = (sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE*)data->buf;
	Player* plr = PlayerManager::getPlayer(sock);

	INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, resp);

	CNSocket* otherSock = sock;

	resp.iFromPCUID = plr->PCStyle.iPC_UID;
	resp.iToPCUID = pkt->iBuddyPCUID;
	resp.iEmoteCode = pkt->iEmoteCode;
	std::cout << "Player UID: ";
	std::cout << plr->PCStyle.iPC_UID << std::endl;
	memcpy(resp.szFreeChat, pkt->szFreeChat, sizeof(pkt->szFreeChat));
	sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC));

	for (auto pair : PlayerManager::players) {
		if (pair.second.plr->PCStyle.iPC_UID != plr->PCStyle.iPC_UID) {
			otherSock = pair.first;
			recvBuddyFreechat(otherSock, pair.second.plr->PCStyle.iPC_UID, pkt); 
		}
	}

	//recvBuddyFreechat(otherSock, plr->PCStyle.iPC_UID, pkt);

}

//Buddy menuchat
void BuddyManager::reqBuddyMenuchat(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE))
		return; // malformed packet

	sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE* pkt = (sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE*)data->buf;
	Player* plr = PlayerManager::getPlayer(sock);

	INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, resp);

	resp.iFromPCUID = plr->PCStyle.iPC_UID;
	resp.iToPCUID = pkt->iBuddyPCUID;
	resp.iEmoteCode = pkt->iEmoteCode;
	memcpy(resp.szFreeChat, pkt->szFreeChat, sizeof(pkt->szFreeChat));

	sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC));

	CNSocket* otherSock = sock;

	for (auto pair : PlayerManager::players) {
		if (pair.second.plr->PCStyle.iPC_UID == pkt->iBuddyPCUID) {
			otherSock = pair.first;
		}
	}

	otherSock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC));
}

//Getting buddy state
void BuddyManager::reqPktGetBuddyState(CNSocket* sock, CNPacketData* data) {
	INITSTRUCT(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC, resp);
	INITSTRUCT(sBuddyBaseInfo, buddyInfo);

	for (int BuddySlot = 0; BuddySlot < 50; BuddySlot++) {
		resp.aBuddyState[BuddySlot] = buddyInfo.iPCState;
		resp.aBuddyID[BuddySlot] = buddyInfo.iID;
		sock->sendPacket((void*)&resp, P_FE2CL_REP_GET_BUDDY_STATE_SUCC, sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC));
	}
}

#pragma region Helper methods

void BuddyManager::requestedBuddy(CNSocket* sock, Player* plrReq, PlayerView& plr) {
	INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, resp);

	resp.iRequestID = plrReq->iID;
	resp.iBuddyID = plr.plr->iID;

	for (int i = 0; i < 9; i++) {
		resp.szFirstName[i] = plrReq->PCStyle.szFirstName[i];
	}
	for (int i = 0; i < 17; i++) {
		resp.szLastName[i] = plrReq->PCStyle.szLastName[i];
	}

	sock->sendPacket((void*)&resp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER));
	
}

//Buddy list load
void BuddyManager::buddyList(CNSocket* sock, sBuddyBaseInfo BuddyInfo) {

	size_t resplen = sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC) + sizeof(sBuddyBaseInfo);
	uint8_t respbuf[4096];

	memset(respbuf, 0, resplen);

	sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC* resp = (sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC*)respbuf;
	sBuddyBaseInfo* respdata = (sBuddyBaseInfo*)(respbuf + sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC));

	resp->iID = BuddyInfo.iID;
	resp->iPCUID = BuddyInfo.iPCUID;

	for (int i = 0; i < resp->iBuddyCnt && i < resp->iListNum; i++) {
		respdata->iID = BuddyInfo.iID;
		respdata->iPCUID = BuddyInfo.iPCUID;
		respdata->iGender = BuddyInfo.iGender;
		respdata->iPCState = BuddyInfo.iPCState;
		respdata->iNameCheckFlag = BuddyInfo.iNameCheckFlag;
		respdata->bBlocked = 0;
		respdata->bFreeChat = 1;
		for (int fName = 0; fName < 9; fName++) {
			respdata->szFirstName[fName] = BuddyInfo.szFirstName[fName];
		}
		for (int lName = 0; lName < 17; lName++) {
			respdata->szLastName[lName] = BuddyInfo.szLastName[lName];
		}
		//reqGetBuddyState(sock, BuddyInfo, resp->iBuddyCnt);
	}

	sock->sendPacket((void*)respbuf, P_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC, resplen);

}

//If the requested player accepts the buddy request, the requester's buddylist will get loaded up.
void BuddyManager::otherAcceptBuddy(CNSocket* sock, int32_t BuddyID, int64_t BuddyPCUID, sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC resp, Player* plr) {
	
	//resp.iBuddySlot = 0; //hard-coding this for now
	resp.BuddyInfo.iID = BuddyID;
	resp.BuddyInfo.iPCUID = BuddyPCUID;
	resp.BuddyInfo.iNameCheckFlag = plr->PCStyle.iNameCheck;
	resp.BuddyInfo.iPCState = plr->iPCState;
	resp.BuddyInfo.iGender = plr->PCStyle.iGender;
	resp.BuddyInfo.bBlocked = 0;
	resp.BuddyInfo.bFreeChat = 1;

	for (int i = 0; i < 9; i++) {
		resp.BuddyInfo.szFirstName[i] = plr->PCStyle.szFirstName[i];
	}

	for (int i = 0; i < 17; i++) {
		resp.BuddyInfo.szLastName[i] = plr->PCStyle.szLastName[i];
	}

	sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
	buddyList(sock, resp.BuddyInfo);
}

//Getting buddy states
/*void BuddyManager::reqGetBuddyState(CNSocket* sock, sBuddyBaseInfo BuddyInfo, int8_t BuddySlot) {

	INITSTRUCT(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC, resp);

	for (BuddySlot = 0; BuddySlot < 50; BuddySlot++) {
		resp.aBuddyState[BuddySlot] = BuddyInfo.iPCState;
		resp.aBuddyID[BuddySlot] = BuddyInfo.iID;
		sock->sendPacket((void*)&resp, P_FE2CL_REP_GET_BUDDY_STATE_SUCC, sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC));
	}

}*/

//Check if the requested name matches the requested player's name
bool BuddyManager::firstNameCheck(char16_t reqFirstName[], char16_t resFirstName[], int sizeOfReq, int sizeOfRes) {
	// If lengths of array are not equal means 
	// array are not equal 
	if (sizeOfReq != sizeOfRes)
		return false;

	// Sort both arrays 
	std::sort(reqFirstName, reqFirstName + sizeOfReq);
	std::sort(resFirstName, resFirstName + sizeOfRes);

	// Linearly compare elements 
	for (int i = 0; i < sizeOfReq; i++)
		if (reqFirstName[i] != resFirstName[i])
			return false;

	// If all elements were same. 
	return true;
}

bool BuddyManager::lastNameCheck(char16_t reqLastName[], char16_t resLastName[], int sizeOfLNReq, int sizeOfLNRes) {
	// If lengths of array are not equal means 
	// array are not equal 
	if (sizeOfLNReq != sizeOfLNRes)
		return false;

	// Sort both arrays 
	std::sort(reqLastName, reqLastName + sizeOfLNReq);
	std::sort(resLastName, resLastName + sizeOfLNRes);

	// Linearly compare elements 
	for (int i = 0; i < sizeOfLNReq; i++)
		if (reqLastName[i] != resLastName[i])
			return false;

	// If all elements were same. 
	return true;
}

void BuddyManager::recvBuddyFreechat(CNSocket* sock, int64_t senderPCUID, sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE* pkt) {
	INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, resp);
	resp.iFromPCUID = pkt->iBuddyPCUID;
	resp.iToPCUID = senderPCUID;
	resp.iEmoteCode = pkt->iEmoteCode;
	memcpy(resp.szFreeChat, pkt->szFreeChat, sizeof(pkt->szFreeChat));
	std::cout << "Buddy UiD ";
	std::cout << resp.iToPCUID << std::endl;
	sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC));
}

#pragma endregion
