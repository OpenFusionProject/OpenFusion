#ifndef _CNLS_HPP
#define _CNLS_HPP

#include "CNProtocol.hpp"
#include "Defines.hpp"
#include "Player.hpp"

#include <map>

struct CNLoginData {
    std::map<int64_t, Player> characters;
    int64_t selectedChar;
};

// WARNING: THERE CAN ONLY BE ONE OF THESE SERVERS AT A TIME!!!!!! TODO: change loginSessions & packet handlers to be non-static
class CNLoginServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);
    static std::map<CNSocket*, CNLoginData> loginSessions;

public:
    CNLoginServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);
};

#endif
