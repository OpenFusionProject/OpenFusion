#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNProtocol.hpp"

namespace Buddies {
	void init();

	// Buddy list
	void refreshBuddyList(CNSocket* sock);
}
