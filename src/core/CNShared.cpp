#include "core/CNShared.hpp"

static std::unordered_map<int64_t, LoginMetadata*> logins;
static std::mutex mtx;

void CNShared::storeLoginMetadata(int64_t sk, LoginMetadata *lm) {
    std::lock_guard<std::mutex> lock(mtx);

    // take ownership of connection data
    logins[sk] = lm;
}

LoginMetadata* CNShared::getLoginMetadata(int64_t sk) {
    std::lock_guard<std::mutex> lock(mtx);

    // fail if the key isn't found
    if (logins.find(sk) == logins.end())
        return nullptr;

    // transfer ownership of connection data to shard
    LoginMetadata *lm = logins[sk];
    logins.erase(sk);

    return lm;
}

void CNShared::pruneLoginMetadata(CNServer *serv, time_t currTime) {
    std::lock_guard<std::mutex> lock(mtx);

    auto it = logins.begin();
    while (it != logins.end()) {
        auto& sk = it->first;
        auto& lm = it->second;

        if (lm->timestamp + CNSHARED_TIMEOUT > currTime) {
            std::cout << "[WARN] Pruning hung connection attempt" << std::endl;

            // deallocate object and remove map entry
            delete logins[sk];
            it = logins.erase(it); // skip the invalidated iterator
        } else {
            it++;
        }
    }
}
