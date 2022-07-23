#include "core/CNShared.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

static std::unordered_map<int64_t, LoginMetadata*> login;
static std::mutex mtx;

void CNShared::storeLoginMetadata(int64_t sk, LoginMetadata *lm) {
    std::lock_guard<std::mutex> lock(mtx);

    // take ownership of connection data
    login[sk] = lm;
}

LoginMetadata* CNShared::getLoginMetadata(int64_t sk) {
    std::lock_guard<std::mutex> lock(mtx);

    // fail if the key isn't found
    if (login.find(sk) == login.end())
        return nullptr;

    // transfer ownership of connection data to shard
    LoginMetadata *lm = login[sk];
    login.erase(sk);

    return lm;
}

void CNShared::pruneLoginMetadata(CNServer *serv, time_t currTime) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = login.begin();
    while (it != login.end()) {
        auto& sk = it->first;
        auto& lm = it->second;

        if (lm->timestamp + CNSHARED_TIMEOUT > currTime) {
            std::cout << "[WARN] Pruning hung connection attempt" << std::endl;

            // deallocate object and remove map entry
            delete login[sk];
            it = login.erase(it); // skip the invalidated iterator
        } else {
            it++;
        }
    }
}
