#pragma once

#include "core/Core.hpp"

#include "Player.hpp"
#include "Chunking.hpp"
#include "Transport.hpp"
#include "Entities.hpp"

#include <map>
#include <list>

namespace PlayerManager {
    extern std::map<CNSocket*, Player*> players;
    void init();

    void removePlayer(CNSocket* key);

    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, uint64_t I, int angle);
    void updatePlayerPositionForWarp(CNSocket* sock, int X, int Y, int Z, uint64_t inst);

    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I);
    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z);

    Player *getPlayer(CNSocket* key);
    std::string getPlayerName(Player *plr, bool id=true);

    bool isAccountInUse(int accountId);
    void exitDuplicate(int accountId);
    Player *getPlayerFromID(int32_t iID);
    CNSocket *getSockFromID(int32_t iID);
    CNSocket *getSockFromName(std::string firstname, std::string lastname);
    CNSocket *getSockFromAny(int by, int id, int uid, std::string firstname, std::string lastname);
    WarpLocation *getRespawnPoint(Player *plr);

    void sendToViewable(CNSocket *sock, void* buf, uint32_t type, size_t size);

    // TODO: unify this under the new Entity system
    template<class T>
    void sendToViewable(CNSocket *sock, T& pkt, uint32_t type) {
        Player* plr = getPlayer(sock);
        for (auto it = plr->viewableChunks.begin(); it != plr->viewableChunks.end(); it++) {
            Chunk* chunk = *it;
            for (const EntityRef& ref : chunk->entities) {
                if (ref.kind != EntityKind::PLAYER || ref.sock == sock)
                    continue;

                ref.sock->sendPacket(pkt, type);
            }
        }
    }
}
