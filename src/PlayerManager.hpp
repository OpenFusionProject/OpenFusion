#pragma once

#include "Player.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include <map>
#include <list>

struct PlayerView {
    std::list<CNSocket*> viewable;
    std::list<int32_t> viewableNPCs;
    Player plr;
    uint64_t lastHeartbeat;
};


namespace PlayerManager {
    extern std::map<CNSocket*, PlayerView> players;
    void init();

    void addPlayer(CNSocket* key, Player plr);
    void removePlayer(CNSocket* key);
    Player getPlayer(CNSocket* key);

    void updatePlayer(CNSocket* key, Player plr);
    void updatePlayerPosition(CNSocket* sock, int X, int Y, int Z);
    std::list<CNSocket*> getNearbyPlayers(int X, int Y, int dist);

    void enterPlayer(CNSocket* sock, CNPacketData* data);
    void loadPlayer(CNSocket* sock, CNPacketData* data);
    void movePlayer(CNSocket* sock, CNPacketData* data);
    void stopPlayer(CNSocket* sock, CNPacketData* data);
    void jumpPlayer(CNSocket* sock, CNPacketData* data);
    void jumppadPlayer(CNSocket* sock, CNPacketData* data);
    void launchPlayer(CNSocket* sock, CNPacketData* data);
    void ziplinePlayer(CNSocket* sock, CNPacketData* data);
    void movePlatformPlayer(CNSocket* sock, CNPacketData* data);
    void moveSlopePlayer(CNSocket* sock, CNPacketData* data);
    void gotoPlayer(CNSocket* sock, CNPacketData* data);
    void setSpecialPlayer(CNSocket* sock, CNPacketData* data);
    void heartbeatPlayer(CNSocket* sock, CNPacketData* data);
    void revivePlayer(CNSocket* sock, CNPacketData* data);
    void exitGame(CNSocket* sock, CNPacketData* data);

    void setSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data);

    void enterPlayerVehicle(CNSocket* sock, CNPacketData* data);
    void exitPlayerVehicle(CNSocket* sock, CNPacketData* data);
}
