#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include <map>
#include <list>

namespace BuddyManager {
	void init();
	
	void requestBuddy(CNSocket* sock, CNPacketData* data);
	void reqBuddyByName(CNSocket* sock, CNPacketData* data);
	void reqAcceptBuddy(CNSocket* sock, CNPacketData* data);
	void reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data);
	void reqBuddyFreechat(CNSocket* sock, CNPacketData* data);
	void reqBuddyMenuchat(CNSocket* sock, CNPacketData* data);
	void reqPktGetBuddyState(CNSocket* sock, CNPacketData* data);

	//helper methods
	void requestedBuddy(CNSocket* sock, Player* plrReq, PlayerView& plr);
	void buddyList(CNSocket* sock, sBuddyBaseInfo BuddyInfo);
	void otherAcceptBuddy(CNSocket* sock, int32_t BuddyID, int64_t BuddyPCUID, sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC resp, Player* plr);
	void reqGetBuddyState(CNSocket* sock, sBuddyBaseInfo BuddyInfo, int8_t BuddySlot);
	void recvBuddyFreechat(CNSocket* sock, int64_t senderPCUID, sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE* pkt);

	bool firstNameCheck(char16_t reqFirstName[], char16_t resFirstName[], int sizeOfReq, int sizeOfRes);
	bool lastNameCheck(char16_t reqLastName[], char16_t resLastName[], int sizeOfLNReq, int sizeOfLNRes);
}
