#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNProtocol.hpp"

namespace BuddyManager {
	void init();

	// Buddy list
	void refreshBuddyList(CNSocket* sock);
}
