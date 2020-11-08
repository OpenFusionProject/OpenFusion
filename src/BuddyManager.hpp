#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include <map>
#include <list>

namespace BuddyManager {
	void init();

	// Buddy requests
	void requestBuddy(CNSocket* sock, CNPacketData* data);
	void reqBuddyByName(CNSocket* sock, CNPacketData* data);

	// Buddy accepting
	void reqAcceptBuddy(CNSocket* sock, CNPacketData* data);
	void reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data);

	// Buddy Messaging
	void reqBuddyFreechat(CNSocket* sock, CNPacketData* data);
	void reqBuddyMenuchat(CNSocket* sock, CNPacketData* data);

	// Getting buddy state
	void reqPktGetBuddyState(CNSocket* sock, CNPacketData* data);

	// Blocking/removing buddies
	void reqBuddyBlock(CNSocket* sock, CNPacketData* data);
	void reqBuddyDelete(CNSocket* sock, CNPacketData* data);

	// Buddy warping
	void reqBuddyWarp(CNSocket* sock, CNPacketData* data);

	// helper methods
	
	// Name checks
	int getAvailableBuddySlot(Player* plr);
	bool NameCheck(char16_t reqName[], char16_t resName[], int sizeOfReq, int sizeOfRes); // checks if the request and requested player's names match
}
