#ifndef _CNSS_HPP
#define _CNSS_HPP

#include "CNProtocol.hpp"
#include "Defines.hpp"

#include <map>

#define REGISTER_SHARD_PACKET(pactype, handlr) CNShardServer::ShardPackets[pactype] = handlr;

class CNShardServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);

public:
    static std::map<uint32_t, PacketHandler> ShardPackets;

    CNShardServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);
    void onTimer();
};

#endif
