#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include <map>
#include <list>

namespace BuddyManager {
	void init();
	
	//Buddy requests
	void requestBuddy(CNSocket* sock, CNPacketData* data);
	void reqBuddyByName(CNSocket* sock, CNPacketData* data);
	
	//Buddy accepting
	void reqAcceptBuddy(CNSocket* sock, CNPacketData* data);
	void reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data);
	
	//Buddy Messaging
	void reqBuddyFreechat(CNSocket* sock, CNPacketData* data);
	void reqBuddyMenuchat(CNSocket* sock, CNPacketData* data);
	
	//Getting buddy state
	void reqPktGetBuddyState(CNSocket* sock, CNPacketData* data);
	
	//Blocking/removing buddies
	void reqBuddyBlock(CNSocket* sock, CNPacketData* data);
	void reqBuddyDelete(CNSocket* sock, CNPacketData* data);

	//Buddy warping
	void reqBuddyWarp(CNSocket* sock, CNPacketData* data);

	//helper methods
	void requestedBuddy(CNSocket* sock, Player* plrReq, PlayerView& plr);
	void buddyList(CNSocket* sock, sBuddyBaseInfo BuddyInfo); //updates the buddylist
	void otherAcceptBuddy(CNSocket* sock, int32_t BuddyID, int64_t BuddyPCUID, sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC resp, Player* plr); //tells the other player that they are now buddies with the requester.

	//Name checks
	bool firstNameCheck(char16_t reqFirstName[], char16_t resFirstName[], int sizeOfReq, int sizeOfRes); //checks if the request and requested player's first names match
	bool lastNameCheck(char16_t reqLastName[], char16_t resLastName[], int sizeOfLNReq, int sizeOfLNRes); //checks if the request and requested player's last names match
}
