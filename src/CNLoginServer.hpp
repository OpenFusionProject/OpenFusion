#ifndef _CNLS_HPP
#define _CNLS_HPP

#include "CNProtocol.hpp"
#include "Player.hpp"

#include <map>

enum LOGINPACKETID {
    // client to login server
    P_CL2LS_REQ_LOGIN = 301989889,
    P_CL2LS_REQ_CHECK_CHAR_NAME = 301989890,
    P_CL2LS_REQ_SAVE_CHAR_NAME = 301989891,
    P_CL2LS_REQ_CHAR_CREATE = 301989892,
    P_CL2LS_REQ_CHAR_SELECT = 301989893,
    P_CL2LS_REQ_SHARD_SELECT = 301989895,
    P_CL2LS_REP_LIVE_CHECK = 301989900,
    P_CL2LS_REQ_SHARD_LIST_INFO = 301989896,

    // login server 2 client
    P_LS2CL_REP_LOGIN_SUCC = 553648129,
    P_LS2CL_REP_CHAR_INFO = 553648131,
    P_LS2CL_REP_CHECK_CHAR_NAME_SUCC = 553648133,
    P_LS2CL_REP_SAVE_CHAR_NAME_SUCC = 553648135,
    P_LS2CL_REP_CHAR_CREATE_SUCC = 553648137,
    P_LS2CL_REP_CHAR_SELECT_SUCC = 553648139,
    P_LS2CL_REP_SHARD_SELECT_SUCC = 553648143,
    P_LS2CL_REQ_LIVE_CHECK = 553648150,
    P_LS2CL_REP_SHARD_LIST_INFO_SUCC = 553648153
};

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

    void killConnection(CNSocket* cns);
};

#endif