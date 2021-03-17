#include "core/CNShared.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif
std::map<int64_t, Player> CNSharedData::players;
std::mutex playerCrit;

void CNSharedData::setPlayer(int64_t sk, Player& plr) {
    std::lock_guard<std::mutex> lock(playerCrit); // the lock will be removed when the function ends

    players[sk] = plr;
}

Player CNSharedData::getPlayer(int64_t sk) {
    std::lock_guard<std::mutex> lock(playerCrit); // the lock will be removed when the function ends

    return players[sk];
}

void CNSharedData::erasePlayer(int64_t sk) {
    std::lock_guard<std::mutex> lock(playerCrit); // the lock will be removed when the function ends

    players.erase(sk);
}
