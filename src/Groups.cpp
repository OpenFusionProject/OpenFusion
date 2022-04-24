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

void Groups::addToGroup(EntityRef member, Group* group) {
    if (member.kind == EntityKind::PLAYER) {
        Player* plr = PlayerManager::getPlayer(member.sock);
        plr->group = group;
    }
    else if (member.kind == EntityKind::COMBAT_NPC) {
        CombatNPC* npc = (CombatNPC*)member.getEntity();
        npc->group = group;
    }
    else {
        std::cout << "[WARN] Adding a weird entity type to a group" << std::endl;
    }

    group->members.push_back(member);
}

void Groups::removeFromGroup(EntityRef member, Group* group) {
    if (member.kind == EntityKind::PLAYER) {
        Player* plr = PlayerManager::getPlayer(member.sock);
        plr->group = nullptr; // no dangling pointers here muahaahahah
    }
    else if (member.kind == EntityKind::COMBAT_NPC) {
        CombatNPC* npc = (CombatNPC*)member.getEntity();
        npc->group = nullptr;
    }
    else {
        std::cout << "[WARN] Adding a weird entity type to a group" << std::endl;
    }

    auto it = std::find(group->members.begin(), group->members.end(), member);
    if (it == group->members.end()) {
        std::cout << "[WARN] Tried to remove a member that isn't in the group" << std::endl;
        return;
    }

    group->members.erase(it);

    if (group->members.empty()) delete group; // cleanup memory
}

void Groups::disbandGroup(Group* group) {
    // remove everyone from the group!!
    std::vector<EntityRef> members = group->members;
    for (EntityRef member : members) {
        removeFromGroup(member, group);
    }
}

static void requestGroup(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_GROUP_INVITE* recv = (sP_CL2FE_REQ_PC_GROUP_INVITE*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recv->iID_To);

    if (otherPlr == nullptr)
        return;

    // fail if the group is full or the other player is already in a group
    if ((plr->group != nullptr && plr->group->filter(EntityKind::PLAYER).size() >= 4) || otherPlr->group != nullptr) {
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

    int size = otherPlr->group == nullptr ? 1 : otherPlr->group->filter(EntityKind::PLAYER).size();

    // fail if the group is full or the other player is already in a group
    if (plr->group != nullptr || size + 1 > 4) {
        INITSTRUCT(sP_FE2CL_PC_GROUP_JOIN_FAIL, resp);
        sock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_JOIN_FAIL, sizeof(sP_FE2CL_PC_GROUP_JOIN_FAIL));
        return;
    }

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_JOIN), size + 1, sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_JOIN packet size\n";
        return;
    }

    if (otherPlr->group == nullptr) {
        // create group
        otherPlr->group = new Group(); // spooky
        addToGroup(PlayerManager::getSockFromID(recv->iID_From), otherPlr->group);
    }
    addToGroup(sock, otherPlr->group);
    auto players = otherPlr->group->filter(EntityKind::PLAYER);

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_JOIN) + players.size() * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_JOIN *resp = (sP_FE2CL_PC_GROUP_JOIN*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_JOIN));

    resp->iID_NewMember = plr->iID;
    resp->iMemberPCCnt = players.size();

    int bitFlag = otherPlr->group->conditionBitFlag;
    for (int i = 0; i < players.size(); i++) {

        Player* varPlr = PlayerManager::getPlayer(players[i].sock);
        CNSocket* sockTo = players[i].sock;

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
            // TODO ABILITIES
            /*if (Abilities::SkillTable[varPlr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Abilities::applyBuff(sock, varPlr->Nanos[varPlr->activeNano].iSkillID, 1, 1, bitFlag);
            if (Abilities::SkillTable[plr->Nanos[plr->activeNano].iSkillID].targetType == 3)
                Abilities::applyBuff(sockTo, plr->Nanos[plr->activeNano].iSkillID, 1, 1, bitFlag);*/
        }
    }

    Groups::sendToGroup(otherPlr->group, (void*)&respbuf, P_FE2CL_PC_GROUP_JOIN, resplen);
}

static void leaveGroup(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    groupKick(plr);
}

void Groups::sendToGroup(Group* group, void* buf, uint32_t type, size_t size) {
    auto players = group->filter(EntityKind::PLAYER);
    for (int i = 0; i < players.size(); i++) {
        CNSocket* sock = players[i].sock;
        sock->sendPacket(buf, type, size);
    }
}

void Groups::groupTickInfo(Player* plr) {

    auto players = plr->group->filter(EntityKind::PLAYER);

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO), players.size(), sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_JOIN packet size\n";
        return;
    }

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) + players.size() * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_MEMBER_INFO *resp = (sP_FE2CL_PC_GROUP_MEMBER_INFO*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO));

    resp->iID = plr->iID;
    resp->iMemberPCCnt = players.size();

    for (int i = 0; i < players.size(); i++) {
        EntityRef member = players[i];
        Player* varPlr = PlayerManager::getPlayer(member.sock);

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

    sendToGroup(plr->group, (void*)&respbuf, P_FE2CL_PC_GROUP_MEMBER_INFO, resplen);
}

static void groupUnbuff(Player* plr) {
    Group* group = plr->group;
    for (int i = 0; i < group->members.size(); i++) {
        for (int n = 0; n < group->members.size(); n++) {
            if (i == n)
                continue;

            EntityRef other = group->members[n];

            // TODO ABILITIES
            //Abilities::applyBuff(sock, otherPlr->Nanos[otherPlr->activeNano].iSkillID, 2, 1, 0);
        }
    }
}

void Groups::groupKick(Player* plr) {
    Group* group = plr->group;

    // if you are the group leader, destroy your own group and kick everybody
    if (plr->group->members[0] == PlayerManager::getSockFromID(plr->iID)) {
        groupUnbuff(plr);
        INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, resp1);
        sendToGroup(plr->group, (void*)&resp1, P_FE2CL_PC_GROUP_LEAVE_SUCC, sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC));
        disbandGroup(plr->group);
        return;
    }

    auto players = group->filter(EntityKind::PLAYER);

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_LEAVE), players.size() - 1, sizeof(sPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_LEAVE packet size\n";
        return;
    }

    size_t resplen = sizeof(sP_FE2CL_PC_GROUP_LEAVE) + (players.size() - 1) * sizeof(sPCGroupMemberInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GROUP_LEAVE *resp = (sP_FE2CL_PC_GROUP_LEAVE*)respbuf;
    sPCGroupMemberInfo *respdata = (sPCGroupMemberInfo*)(respbuf+sizeof(sP_FE2CL_PC_GROUP_LEAVE));

    resp->iID_LeaveMember = plr->iID;
    resp->iMemberPCCnt = players.size() - 1;

    int bitFlag = 0; // TODO ABILITIES getGroupFlags(otherPlr) & ~plr->iGroupConditionBitFlag;

    CNSocket* sock = PlayerManager::getSockFromID(plr->iID);

    if (sock == nullptr)
        return;

    removeFromGroup(sock, group);

    players = group->filter(EntityKind::PLAYER);
    for (int i = 0; i < players.size(); i++) {
        CNSocket* sockTo = players[i].sock;
        Player* varPlr = PlayerManager::getPlayer(sock);

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
        // respdata[i]].iMapType = 0;
        // respdata[i]].iMapNum = 0;
        respdata[i].iX = varPlr->x;
        respdata[i].iY = varPlr->y;
        respdata[i].iZ = varPlr->z;
        // client doesnt read nano data here

        // remove the leaving member's buffs from the group and remove the group buffs from the leaving member.
            // TODO ABILITIES
          /*if (Abilities::SkillTable[varPlr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Abilities::applyBuff(sock, varPlr->Nanos[varPlr->activeNano].iSkillID, 2, 1, 0);
            if (Abilities::SkillTable[plr->Nanos[varPlr->activeNano].iSkillID].targetType == 3)
                Abilities::applyBuff(sockTo, plr->Nanos[plr->activeNano].iSkillID, 2, 1, bitFlag);*/
    }

    sendToGroup(group, (void*)&respbuf, P_FE2CL_PC_GROUP_LEAVE, resplen);

    INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, resp1);
    sock->sendPacket((void*)&resp1, P_FE2CL_PC_GROUP_LEAVE_SUCC, sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC));
}

void Groups::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE, requestGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE_REFUSE, refuseGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_JOIN, joinGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_LEAVE, leaveGroup);
}
