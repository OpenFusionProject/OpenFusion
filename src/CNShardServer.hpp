#ifndef _CNSS_HPP
#define _CNSS_HPP

#include "CNProtocol.hpp"

#include <map>

enum SHARDPACKETID {
    // client 2 shard
    P_CL2FE_REQ_PC_ENTER = 318767105,
    P_CL2FE_REQ_PC_LOADING_COMPLETE = 318767245,
    P_CL2FE_REP_LIVE_CHECK = 318767221,
    P_CL2FE_REQ_NANO_ACTIVE = 318767119,
    P_CL2FE_REQ_PC_MOVE = 318767107,
    P_CL2FE_REQ_PC_STOP = 318767108,
    P_CL2FE_REQ_PC_JUMP = 318767109,
    P_CL2FE_REQ_PC_JUMPPAD = 318767165,
    P_CL2FE_REQ_PC_LAUNCHER = 318767166,
    P_CL2FE_REQ_PC_ZIPLINE = 318767167,
    P_CL2FE_REQ_PC_MOVEPLATFORM = 318767168,
    P_CL2FE_REQ_PC_SLOPE = 318767169,
    P_CL2FE_REQ_PC_GOTO = 318767124,
    P_CL2FE_GM_REQ_PC_SET_VALUE = 318767211,
    P_CL2FE_REQ_SEND_FREECHAT_MESSAGE = 318767111,
    P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT = 318767184,
    P_CL2FE_REQ_ITEM_MOVE = 318767114,
    P_CL2FE_REQ_PC_ITEM_DELETE = 318767129,
    P_CL2FE_REQ_PC_GIVE_ITEM = 318767130,
    P_CL2FE_REQ_PC_EXIT = 318767106,

    // shard 2 client
    P_FE2CL_REP_NANO_ACTIVE_SUCC = 822083624,
    P_FE2CL_REP_PC_ENTER_SUCC = 822083586,
    P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC = 822083833,
    P_FE2CL_REP_PC_GOTO_SUCC = 822083633,
    P_FE2CL_REQ_LIVE_CHECK = 822083792,
    P_FE2CL_PC_NEW = 822083587,
    P_FE2CL_PC_MOVE = 822083592,
    P_FE2CL_PC_STOP = 822083593,
    P_FE2CL_PC_JUMP = 822083594,
    P_FE2CL_PC_EXIT = 822083590,
    P_FE2CL_PC_JUMPPAD = 822083701,
    P_FE2CL_PC_LAUNCHER = 822083702,
    P_FE2CL_PC_ZIPLINE = 822083703,
    P_FE2CL_PC_MOVEPLATFORM = 822083704,
    P_FE2CL_PC_SLOPE = 822083705,
    P_FE2CL_NPC_ENTER = 822083595,
    P_FE2CL_NPC_EXIT = 822083596,
    P_FE2CL_ANNOUNCE_MSG = 822083778,
    P_FE2CL_GM_REP_PC_SET_VALUE = 822083781,
    P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC = 822083602,
    P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT = 822083730,
    P_FE2CL_PC_ITEM_MOVE_SUCC = 822083610,
    P_FE2CL_PC_EQUIP_CHANGE = 822083611,
    P_FE2CL_REP_PC_ITEM_DELETE_SUCC = 822083641,
    P_FE2CL_REP_PC_GIVE_ITEM_SUCC = 822083681,
    P_FE2CL_REP_PC_EXIT_SUCC = 822083589,
    P_FE2CL_REP_PC_CHANGE_LEVEL = 822083786,
    P_FE2CL_PC_MOTD_LOGIN = 822083793
};

#define REGISTER_SHARD_PACKET(pactype, handlr) CNShardServer::ShardPackets[pactype] = handlr;

class CNShardServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);

public:
    static std::map<uint32_t, PacketHandler> ShardPackets;

    CNShardServer(uint16_t p);

    void killConnection(CNSocket* cns);
    void onTimer();
};

#endif
