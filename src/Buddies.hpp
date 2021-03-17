#pragma once

#include "Player.hpp"
#include "core/Core.hpp"
#include "core/Core.hpp"

namespace Buddies {
	void init();

	// Buddy list
	void refreshBuddyList(CNSocket* sock);
}
