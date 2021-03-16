#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"
#include "ChunkManager.hpp"

#include <utility>
#include <map>
#include <list>

struct WarpLocation;

namespace PlayerManager {
    extern std::map<CNSocket*, Player*> players;
    void init();

    void removePlayer(CNSocket* key);

    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, uint64_t I, int angle);

    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I);
    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z);

    void sendToViewable(CNSocket* sock, void* buf, uint32_t type, size_t size);

    Player *getPlayer(CNSocket* key);
    std::string getPlayerName(Player *plr, bool id=true);

    bool isAccountInUse(int accountId);
    void exitDuplicate(int accountId);
    Player *getPlayerFromID(int32_t iID);
    CNSocket *getSockFromID(int32_t iID);
    CNSocket *getSockFromName(std::string firstname, std::string lastname);
    CNSocket *getSockFromAny(int by, int id, int uid, std::string firstname, std::string lastname);
}
