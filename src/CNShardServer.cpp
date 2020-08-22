#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "CNShared.hpp"

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
    if (ShardPackets.find(data->type) != ShardPackets.end())
        ShardPackets[data->type](sock, data);
    else
        std::cerr << "OpenFusion: SHARD UNIMPLM ERR. PacketType: " << Defines::p2str(CL2FE, data->type) << " (" << data->type << ")" << std::endl;
}

void CNShardServer::killConnection(CNSocket* cns) {
    // remove from CNSharedData 
    Player cachedPlr = PlayerManager::getPlayer(cns);
    PlayerManager::removePlayer(cns);

    CNSharedData::erasePlayer(cachedPlr.SerialKey);
}

void CNShardServer::onTimer() {
    long int currTime = getTime();

    auto cachedPlayers = PlayerManager::players;

    for (auto pair : cachedPlayers) {
        if (pair.second.lastHeartbeat != 0 && currTime - pair.second.lastHeartbeat > 4000) { // if the client hadn't responded in 4 seconds, its a dead connection so throw it out
            pair.first->kill();
            continue;
        }

        // passed the heartbeat, send another
        sP_FE2CL_REQ_LIVE_CHECK* data = (sP_FE2CL_REQ_LIVE_CHECK*)xmalloc(sizeof(sP_FE2CL_REQ_LIVE_CHECK));
        pair.first->sendPacket(new CNPacketData((void*)data, P_FE2CL_REQ_LIVE_CHECK, sizeof(sP_FE2CL_REQ_LIVE_CHECK), pair.first->getFEKey()));
    }
}
