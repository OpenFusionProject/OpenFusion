#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "CNShared.hpp"
#include "settings.hpp"

#include <iostream>
#include <sstream>
#include <cstdlib>

std::map<uint32_t, PacketHandler> CNShardServer::ShardPackets;
std::list<TimerEvent> CNShardServer::Timers;

CNShardServer::CNShardServer(uint16_t p) {
    port = p;
    pHandler = &CNShardServer::handlePacket;
    REGISTER_SHARD_TIMER(keepAliveTimer, 2000);
    init();
}

void CNShardServer::handlePacket(CNSocket* sock, CNPacketData* data) {
    printPacket(data, CL2FE);

    if (ShardPackets.find(data->type) != ShardPackets.end())
        ShardPackets[data->type](sock, data);
    else if (settings::VERBOSITY)
        std::cerr << "OpenFusion: SHARD UNIMPLM ERR. PacketType: " << Defines::p2str(CL2FE, data->type) << " (" << data->type << ")" << std::endl;
}

void CNShardServer::keepAliveTimer(CNServer* serv,  uint64_t currTime) {
    std::cout << "keep alive called! " << currTime << std::endl;
    auto cachedPlayers = PlayerManager::players;

    for (auto pair : cachedPlayers) {
        if (pair.second.lastHeartbeat != 0 && currTime - pair.second.lastHeartbeat > 4000) { // if the client hadn't responded in 4 seconds, its a dead connection so throw it out
            pair.first->kill();
            continue;
        }

        // passed the heartbeat, send another
        INITSTRUCT(sP_FE2CL_REQ_LIVE_CHECK, data);
        pair.first->sendPacket((void*)&data, P_FE2CL_REQ_LIVE_CHECK, sizeof(sP_FE2CL_REQ_LIVE_CHECK));
    }
}

void CNShardServer::newConnection(CNSocket* cns) {
    cns->setActiveKey(SOCKETKEY_E); // by default they accept keys encrypted with the default key
}

void CNShardServer::killConnection(CNSocket* cns) {
    // remove from CNSharedData
    Player cachedPlr = PlayerManager::getPlayer(cns);
    PlayerManager::removePlayer(cns);

    CNSharedData::erasePlayer(cachedPlr.SerialKey);
}

void CNShardServer::onStep() {
    uint64_t currTime = getTime();

    for (TimerEvent& event : Timers) {
        if (event.scheduledEvent == 0) {
            // event hasn't been queued yet, go ahead and do that
            event.scheduledEvent = currTime + event.delta;
            continue;
        }

        if (event.scheduledEvent < currTime) {
            // timer needs to be called
            event.handlr(this, currTime);
            event.scheduledEvent = currTime + event.delta;
        }
    }
}
