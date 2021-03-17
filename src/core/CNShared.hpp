/*
 * core/CNShared.hpp
 *     There's some data shared between the Login Server and the Shard Server. Of course all of this needs to be thread-safe. No mucking about on this one!
 */

#pragma once

#include <map>
#include <string>

#include "Player.hpp"

namespace CNSharedData {
    // serialkey corresponds to player data
    extern std::map<int64_t, Player> players;

    void setPlayer(int64_t sk, Player& plr);
    Player getPlayer(int64_t sk);
    void erasePlayer(int64_t sk);
}
