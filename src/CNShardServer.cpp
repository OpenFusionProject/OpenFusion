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

CNShardServer::CNShardServer(uint16_t p) {
    port = p;
    pHandler = &CNShardServer::handlePacket;
    init();
}

void CNShardServer::handlePacket(CNSocket* sock, CNPacketData* data) {
    printPacket(data, CL2FE);

    if (ShardPackets.find(data->type) != ShardPackets.end())
        ShardPackets[data->type](sock, data);
    else if (settings::VERBOSITY)
        std::cerr << "OpenFusion: SHARD UNIMPLM ERR. PacketType: " << Defines::p2str(CL2FE, data->type) << " (" << data->type << ")" << std::endl;
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

void CNShardServer::onTimer() {
    uint64_t currTime = getTime();

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
