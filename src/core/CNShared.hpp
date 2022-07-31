/*
 * core/CNShared.hpp
 *     There's some data shared between the Login Server and the Shard Server. Of course all of this needs to be thread-safe. No mucking about on this one!
 */

#pragma once

#include <map>
#include <string>

#include "Player.hpp"

/*
 * Connecions time out after 15 minutes, checked every 30 seconds.
 */
#define CNSHARED_TIMEOUT 900000
#define CNSHARED_PERIOD 30000

struct LoginMetadata {
    uint64_t FEKey;
    Player plr;
    time_t timestamp;
};

namespace CNShared {
    void storeLoginMetadata(int64_t sk, LoginMetadata *lm);
    LoginMetadata* getLoginMetadata(int64_t sk);
    void pruneLoginMetadata(CNServer *serv, time_t currTime);
}
