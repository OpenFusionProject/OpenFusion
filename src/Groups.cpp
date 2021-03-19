#include "servers/CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "Groups.hpp"
#include "Nanos.hpp"
#include "Abilities.hpp"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

/*
 * NOTE: Variadic response packets that list group members are technically
 * double-variadic, as they have two count members with trailing struct counts,
 * and are thus incompatible with the generic sendPacket() wrapper.
 * That means we still have to (carefully) use validOutVarPacket() in this
 * source file.
 */

using namespace Groups;

static void requestGroup(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_GROUP_INVITE* recv = (sP_CL2FE_REQ_PC_GROUP_INVITE*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recv->iID_To);

    if (otherPlr == nullptr)
        return;

    otherPlr = PlayerManager::getPlayerFromID(otherPlr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    // fail if the group is full or the other player is already in a group
    if (plr->groupCnt >= 4 || otherPlr->iIDGroup != otherPlr->iID || otherPlr->groupCnt > 1) {
        INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE_FAIL, resp);
        sock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE_FAIL, sizeof(sP_FE2CL_PC_GROUP_INVITE_FAIL));
        return;
    }

    CNSocket* otherSock = PlayerManager::getSockFromID(recv->iID_To);

    if (otherSock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE, resp);

    resp.iHostID = plr->iID;

    otherSock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE, sizeof(sP_FE2CL_PC_GROUP_INVITE));
}

static void refuseGroup(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE* recv = (sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(recv->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_PC_GROUP_INVITE_REFUSE, resp);

    resp.iID_To = plr->iID;

    otherSock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_INVITE_REFUSE, sizeof(sP_FE2CL_PC_GROUP_INVITE_REFUSE));
}

static void joinGroup(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_GROUP_JOIN* recv = (sP_CL2FE_REQ_PC_GROUP_JOIN*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recv->iID_From);

    if (otherPlr == nullptr)
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

        if (varPlr != plr) { // apply the new member's buffs to the group and the group's buffs to the new member
            if (Nanos::SkillTable[varPlr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Nanos::applyBuff(sock, varPlr->Nanos[varPlr->activeNano].iSkillID, 1, 1, bitFlag);
            if (Nanos::SkillTable[plr->Nanos[plr->activeNano].iSkillID].targetType == 3)
                Nanos::applyBuff(sockTo, plr->Nanos[plr->activeNano].iSkillID, 1, 1, bitFlag);
        }
    }

    sendToGroup(otherPlr, (void*)&respbuf, P_FE2CL_PC_GROUP_JOIN, resplen);
}

static void leaveGroup(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    groupKickPlayer(plr);
}

void Groups::sendToGroup(Player* plr, void* buf, uint32_t type, size_t size) {
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

void Groups::groupTickInfo(Player* plr) {
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

static void groupUnbuff(Player* plr) {
    for (int i = 0; i < plr->groupCnt; i++) {
        for (int n = 0; n < plr->groupCnt; n++) {
            if (i == n)
                continue;

            Player* otherPlr = PlayerManager::getPlayerFromID(plr->groupIDs[i]);
            CNSocket* sock = PlayerManager::getSockFromID(plr->groupIDs[n]);

            Nanos::applyBuff(sock, otherPlr->Nanos[otherPlr->activeNano].iSkillID, 2, 1, 0);
        }
    }
}

void Groups::groupKickPlayer(Player* plr) {
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

    int bitFlag = getGroupFlags(otherPlr) & ~plr->iGroupConditionBitFlag;
    int moveDown = 0;

    CNSocket* sock = PlayerManager::getSockFromID(plr->iID);

    if (sock == nullptr)
        return;

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
        } else { // remove the leaving member's buffs from the group and remove the group buffs from the leaving member.
            if (Nanos::SkillTable[varPlr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Nanos::applyBuff(sock, varPlr->Nanos[varPlr->activeNano].iSkillID, 2, 1, 0);
            if (Nanos::SkillTable[plr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Nanos::applyBuff(sockTo, plr->Nanos[plr->activeNano].iSkillID, 2, 1, bitFlag);
        }
    }

    plr->iIDGroup = plr->iID;
    otherPlr->groupCnt -= 1;

    sendToGroup(otherPlr, (void*)&respbuf, P_FE2CL_PC_GROUP_LEAVE, resplen);

    INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, resp1);
    sock->sendPacket((void*)&resp1, P_FE2CL_PC_GROUP_LEAVE_SUCC, sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC));
}

int Groups::getGroupFlags(Player* plr) {
    int bitFlag = 0;

    for (int i = 0; i < plr->groupCnt; i++) {
        Player* otherPlr = PlayerManager::getPlayerFromID(plr->groupIDs[i]);

        if (otherPlr == nullptr)
            continue;

        bitFlag |= otherPlr->iGroupConditionBitFlag;
    }

    return bitFlag;
}

void Groups::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE, requestGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE_REFUSE, refuseGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_JOIN, joinGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_LEAVE, leaveGroup);
}
