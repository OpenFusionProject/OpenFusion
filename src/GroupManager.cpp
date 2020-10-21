#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "GroupManager.hpp"
#include "NanoManager.hpp"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

void GroupManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE, requestGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE_REFUSE, refuseGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_JOIN, joinGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_LEAVE, leaveGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE, chatGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE, menuChatGroup);
}

void GroupManager::requestGroup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE))
        return; // malformed packet

    sP_CL2FE_REQ_PC_GROUP_INVITE* recv = (sP_CL2FE_REQ_PC_GROUP_INVITE*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recv->iID_To);

    if (plr == nullptr || otherPlr == nullptr)
        return;

    otherPlr = PlayerManager::getPlayerFromID(otherPlr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    // fail if the group is full or the other player is already in a group
    if (plr->groupCnt >= 4 || otherPlr->groupCnt > 1) {
        INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE_FAIL, resp);
        sock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE_FAIL, sizeof(sP_FE2CL_PC_GROUP_INVITE_FAIL));
        return;
    }

    CNSocket* otherSock = PlayerManager::getSockFromID(recv->iID_To);

    if (otherSock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE, resp);

    resp.iHostID = plr->iIDGroup;

    otherSock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE, sizeof(sP_FE2CL_PC_GROUP_INVITE));
}

void GroupManager::refuseGroup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE))
        return; // malformed packet

    sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE* recv = (sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(recv->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE_REFUSE, resp);

    resp.iID_To = plr->iID;

    otherSock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE_REFUSE, sizeof(sP_FE2CL_PC_GROUP_INVITE_REFUSE));
}

void GroupManager::joinGroup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GROUP_JOIN))
        return; // malformed packet

    sP_CL2FE_REQ_PC_GROUP_JOIN* recv = (sP_CL2FE_REQ_PC_GROUP_JOIN*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recv->iID_From);

    if (plr == nullptr || otherPlr == nullptr)
        return;

    otherPlr = PlayerManager::getPlayerFromID(otherPlr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    // fail if the group is full or the other player is already in a group
    if (plr->groupCnt > 1 || plr->iIDGroup != plr->iID || otherPlr->groupCnt >= 4) {
        INITSTRUCT(sP_FE2CL_PC_GROUP_JOIN_FAIL, resp);
        sock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_JOIN_FAIL, sizeof(sP_FE2CL_PC_GROUP_JOIN_FAIL));
        return;
    }

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_JOIN), otherPlr->groupCnt + 1, sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_JOIN packet size\n";
        return;
    }

    int bitFlagBefore = getGroupFlags(otherPlr);

    plr->iIDGroup = otherPlr->iID;
    otherPlr->groupCnt += 1;
    otherPlr->groupIDs[otherPlr->groupCnt-1] = plr->iID;

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_JOIN) + otherPlr->groupCnt * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_JOIN *resp = (sP_FE2CL_PC_GROUP_JOIN*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_JOIN));

    resp->iID_NewMember = plr->iID;
    resp->iMemberPCCnt = otherPlr->groupCnt;

    int bitFlag = getGroupFlags(otherPlr);

    for (int i = 0; i < otherPlr->groupCnt; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(otherPlr->groupIDs[i]);
        CNSocket* sockTo = PlayerManager::getSockFromID(otherPlr->groupIDs[i]);

        if (varPlr == nullptr || sockTo == nullptr)
            continue;

        respdata[i].iPC_ID = varPlr->iID;
        respdata[i].iPCUID = varPlr->PCStyle.iPC_UID;
        respdata[i].iNameCheck = varPlr->PCStyle.iNameCheck;
        memcpy(respdata[i].szFirstName, varPlr->PCStyle.szFirstName, sizeof(varPlr->PCStyle.szFirstName));
        memcpy(respdata[i].szLastName, varPlr->PCStyle.szLastName, sizeof(varPlr->PCStyle.szLastName));
        respdata[i].iSpecialState = varPlr->iSpecialState;
        respdata[i].iLv = varPlr->level;
        respdata[i].iHP = varPlr->HP;
        respdata[i].iMaxHP = PC_MAXHEALTH(varPlr->level);
        //respdata[i].iMapType = 0;
        //respdata[i].iMapNum = 0;
        respdata[i].iX = varPlr->x;
        respdata[i].iY = varPlr->y;
        respdata[i].iZ = varPlr->z;
        // client doesnt read nano data here

        NanoManager::nanoChangeBuff(sockTo, varPlr, bitFlagBefore | varPlr->iConditionBitFlag, bitFlag | varPlr->iConditionBitFlag);
    }

    sendToGroup(otherPlr, (void*)&respbuf, P_FE2CL_PC_GROUP_JOIN, resplen);
}

void GroupManager::leaveGroup(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    groupKickPlayer(plr);
}

void GroupManager::chatGroup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (plr == nullptr || otherPlr == nullptr)
        return;

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC, resp);
    memcpy(resp.szFreeChat, chat->szFreeChat, sizeof(chat->szFreeChat));
    resp.iSendPCID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;
    sendToGroup(otherPlr, (void*)&resp, P_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC));
}

void GroupManager::menuChatGroup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (plr == nullptr || otherPlr == nullptr)
        return;

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC, resp);
    memcpy(resp.szFreeChat, chat->szFreeChat, sizeof(chat->szFreeChat));
    resp.iSendPCID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;
    sendToGroup(otherPlr, (void*)&resp, P_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC));
}

void GroupManager::sendToGroup(Player* plr, void* buf, uint32_t type, size_t size) {
    for (int i = 0; i < plr->groupCnt; i++) {
        CNSocket* sock = PlayerManager::getSockFromID(plr->groupIDs[i]);

        if (sock == nullptr)
            continue;

        if (type == P_FE2CL_PC_GROUP_LEAVE_SUCC) {
            Player* leavingPlr = PlayerManager::getPlayer(sock);
            leavingPlr->iIDGroup = leavingPlr->iID;
        }

        sock->sendPacket(buf, type, size);
    }
}

void GroupManager::groupTickInfo(Player* plr) {
    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO), plr->groupCnt, sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_JOIN packet size\n";
        return;
    }

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) + plr->groupCnt * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_MEMBER_INFO *resp = (sP_FE2CL_PC_GROUP_MEMBER_INFO*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO));

    resp->iID = plr->iID;
    resp->iMemberPCCnt = plr->groupCnt;

    for (int i = 0; i < plr->groupCnt; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(plr->groupIDs[i]);

        if (varPlr == nullptr)
            continue;

        respdata[i].iPC_ID = varPlr->iID;
        respdata[i].iPCUID = varPlr->PCStyle.iPC_UID;
        respdata[i].iNameCheck = varPlr->PCStyle.iNameCheck;
        memcpy(respdata[i].szFirstName, varPlr->PCStyle.szFirstName, sizeof(varPlr->PCStyle.szFirstName));
        memcpy(respdata[i].szLastName, varPlr->PCStyle.szLastName, sizeof(varPlr->PCStyle.szLastName));
        respdata[i].iSpecialState = varPlr->iSpecialState;
        respdata[i].iLv = varPlr->level;
        respdata[i].iHP = varPlr->HP;
        respdata[i].iMaxHP = PC_MAXHEALTH(varPlr->level);
        //respdata[i].iMapType = 0;
        //respdata[i].iMapNum = 0;
        respdata[i].iX = varPlr->x;
        respdata[i].iY = varPlr->y;
        respdata[i].iZ = varPlr->z;
        if (varPlr->activeNano > 0) {
            respdata[i].bNano = 1;
            respdata[i].Nano = varPlr->Nanos[varPlr->activeNano];
        }
    }

    sendToGroup(plr, (void*)&respbuf, P_FE2CL_PC_GROUP_MEMBER_INFO, resplen);
}

void GroupManager::groupKickPlayer(Player* plr) {
    // if you are the group leader, destroy your own group and kick everybody
    if (plr->iID == plr->iIDGroup) {
        groupUnbuff(plr);
        INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, resp1);
        sendToGroup(plr, (void*)&resp1, P_FE2CL_PC_GROUP_LEAVE_SUCC, sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC));
        plr->groupCnt = 1;
        return;
    }

    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_LEAVE), otherPlr->groupCnt - 1, sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_LEAVE packet size\n";
        return;
    }

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_LEAVE) + (otherPlr->groupCnt - 1) * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_LEAVE *resp = (sP_FE2CL_PC_GROUP_LEAVE*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_LEAVE));

    resp->iID_LeaveMember = plr->iID;
    resp->iMemberPCCnt = otherPlr->groupCnt - 1;

    int bitFlagBefore = getGroupFlags(otherPlr);
    int bitFlag = bitFlagBefore & ~plr->iGroupConditionBitFlag;
    int moveDown = 0;

    for (int i = 0; i < otherPlr->groupCnt; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(otherPlr->groupIDs[i]);
        CNSocket* sockTo = PlayerManager::getSockFromID(otherPlr->groupIDs[i]);

        if (varPlr == nullptr || sockTo == nullptr)
            continue;

        if (moveDown == 1)
            otherPlr->groupIDs[i-1] = otherPlr->groupIDs[i];

        respdata[i-moveDown].iPC_ID = varPlr->iID;
        respdata[i-moveDown].iPCUID = varPlr->PCStyle.iPC_UID;
        respdata[i-moveDown].iNameCheck = varPlr->PCStyle.iNameCheck;
        memcpy(respdata[i-moveDown].szFirstName, varPlr->PCStyle.szFirstName, sizeof(varPlr->PCStyle.szFirstName));
        memcpy(respdata[i-moveDown].szLastName, varPlr->PCStyle.szLastName, sizeof(varPlr->PCStyle.szLastName));
        respdata[i-moveDown].iSpecialState = varPlr->iSpecialState;
        respdata[i-moveDown].iLv = varPlr->level;
        respdata[i-moveDown].iHP = varPlr->HP;
        respdata[i-moveDown].iMaxHP = PC_MAXHEALTH(varPlr->level);
        // respdata[i-moveDown]].iMapType = 0;
        // respdata[i-moveDown]].iMapNum = 0;
        respdata[i-moveDown].iX = varPlr->x;
        respdata[i-moveDown].iY = varPlr->y;
        respdata[i-moveDown].iZ = varPlr->z;
        // client doesnt read nano data here

        if (varPlr == plr) {
            moveDown = 1;
            otherPlr->groupIDs[i] = 0;
            NanoManager::nanoChangeBuff(sockTo, varPlr, bitFlagBefore | varPlr->iConditionBitFlag, varPlr->iConditionBitFlag);
        } else
            NanoManager::nanoChangeBuff(sockTo, varPlr, bitFlagBefore | varPlr->iConditionBitFlag, bitFlag | varPlr->iConditionBitFlag);
    }

    plr->iIDGroup = plr->iID;
    otherPlr->groupCnt -= 1;

    sendToGroup(otherPlr, (void*)&respbuf, P_FE2CL_PC_GROUP_LEAVE, resplen);

    CNSocket* sock = PlayerManager::getSockFromID(plr->iID);

    if (sock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, resp1);
    sock->sendPacket((void*)&resp1, P_FE2CL_PC_GROUP_LEAVE_SUCC, sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC));
}

void GroupManager::groupUnbuff(Player* plr) {
    int bitFlag = getGroupFlags(plr);

    for (int i = 0; i < plr->groupCnt; i++) {
        CNSocket* sock = PlayerManager::getSockFromID(plr->groupIDs[i]);

        if (sock == nullptr)
            continue;

        Player* otherPlr = PlayerManager::getPlayer(sock);
        NanoManager::nanoChangeBuff(sock, otherPlr, bitFlag | otherPlr->iConditionBitFlag, otherPlr->iConditionBitFlag);
    }
}

int GroupManager::getGroupFlags(Player* plr) {
    int bitFlag = 0;

    for (int i = 0; i < plr->groupCnt; i++) {
        Player* otherPlr = PlayerManager::getPlayerFromID(plr->groupIDs[i]);

        if (otherPlr == nullptr)
            continue;

        bitFlag |= otherPlr->iGroupConditionBitFlag;
    }

    return bitFlag;
}
