#pragma once

#include "core/Core.hpp"

namespace Buddies {
    void init();

    // Buddy list
    void sendBuddyList(CNSocket* sock);
}
