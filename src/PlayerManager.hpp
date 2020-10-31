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

struct PlayerView {
    std::tuple<int, int, uint64_t> chunkPos;
    std::vector<Chunk*> currentChunks;
    Player *plr;
    time_t lastHeartbeat;
};


namespace PlayerManager {
    extern std::map<CNSocket*, PlayerView> players;
    void init();

    void addPlayer(CNSocket* key, Player plr);
    void removePlayer(CNSocket* key);

    bool removePlayerFromChunks(std::vector<Chunk*> chunks, CNSocket* sock);
    void addPlayerToChunks(std::vector<Chunk*> chunks, CNSocket* sock);

    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z);
    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, int angle);
    void updatePlayerChunk(CNSocket* sock, int X, int Y, uint64_t instanceID);

    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I);
    void sendPlayerTo(CNSocket* sock, int X, int Y, int Z);

    void sendToViewable(CNSocket* sock, void* buf, uint32_t type, size_t size);

    void enterPlayer(CNSocket* sock, CNPacketData* data);
    void loadPlayer(CNSocket* sock, CNPacketData* data);
    void movePlayer(CNSocket* sock, CNPacketData* data);
    void stopPlayer(CNSocket* sock, CNPacketData* data);
    void jumpPlayer(CNSocket* sock, CNPacketData* data);
    void jumppadPlayer(CNSocket* sock, CNPacketData* data);
    void launchPlayer(CNSocket* sock, CNPacketData* data);
    void ziplinePlayer(CNSocket* sock, CNPacketData* data);
    void movePlatformPlayer(CNSocket* sock, CNPacketData* data);
    void moveSliderPlayer(CNSocket* sock, CNPacketData* data);
    void moveSlopePlayer(CNSocket* sock, CNPacketData* data);
    void gotoPlayer(CNSocket* sock, CNPacketData* data);
    void setSpecialPlayer(CNSocket* sock, CNPacketData* data);
    void heartbeatPlayer(CNSocket* sock, CNPacketData* data);
    void revivePlayer(CNSocket* sock, CNPacketData* data);
    void exitGame(CNSocket* sock, CNPacketData* data);

    void setSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data);
    void setGMSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data);
    void changePlayerGuide(CNSocket *sock, CNPacketData *data);

    void enterPlayerVehicle(CNSocket* sock, CNPacketData* data);
    void exitPlayerVehicle(CNSocket* sock, CNPacketData* data);

    Player *getPlayer(CNSocket* key);
    std::string getPlayerName(Player *plr, bool id=true);
    WarpLocation getRespawnPoint(Player *plr);

    bool isAccountInUse(int accountId);
    void exitDuplicate(int accountId);
    void setSpecialState(CNSocket* sock, CNPacketData* data);
    Player *getPlayerFromID(int32_t iID);
    CNSocket *getSockFromID(int32_t iID);
}
