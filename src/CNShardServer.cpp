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
        std::cerr << "OpenFusion: SHARD UNIMPLM ERR. PacketType: " << data->type << std::endl;
}

void CNShardServer::killConnection(CNSocket* cns) {
    // remove from CNSharedData 
    Player cachedPlr = PlayerManager::getPlayer(cns);
    PlayerManager::removePlayer(cns);

    CNSharedData::erasePlayer(cachedPlr.SerialKey);
}