#pragma once

#include "CNProtocol.hpp"
#include "Defines.hpp"

#include <map>

#define REGISTER_SHARD_PACKET(pactype, handlr) CNShardServer::ShardPackets[pactype] = handlr;
#define REGISTER_SHARD_TIMER(handlr, delta) CNShardServer::Timers.push_back(TimerEvent(handlr, delta));

class CNShardServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);

    static void keepAliveTimer(CNServer*, time_t);
    static void periodicSaveTimer(CNServer* serv, time_t currTime);

public:
    static std::map<uint32_t, PacketHandler> ShardPackets;
    static std::list<TimerEvent> Timers;

    CNShardServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);
    void onStep();
};
