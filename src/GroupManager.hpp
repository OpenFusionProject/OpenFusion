#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include <map>
#include <list>

namespace GroupManager {
	void init();

    void requestGroup(CNSocket* sock, CNPacketData* data);
    void refuseGroup(CNSocket* sock, CNPacketData* data);
    void joinGroup(CNSocket* sock, CNPacketData* data);
	void leaveGroup(CNSocket* sock, CNPacketData* data);
    void chatGroup(CNSocket* sock, CNPacketData* data);
    void menuChatGroup(CNSocket* sock, CNPacketData* data);
    void sendToGroup(Player* plr, void* buf, uint32_t type, size_t size);
    void groupTickInfo(Player* plr);
    void groupKickPlayer(Player* plr);
    void groupUnbuff(Player* plr);
    int getGroupFlags(Player* plr);
}
