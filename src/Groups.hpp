#pragma once

#include "Player.hpp"
#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"

#include <map>
#include <list>

namespace Groups {
	void init();

    void sendToGroup(Player* plr, void* buf, uint32_t type, size_t size);
    void groupTickInfo(Player* plr);
    void groupKickPlayer(Player* plr);
    int getGroupFlags(Player* plr);
}
