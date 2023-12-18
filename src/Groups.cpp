#include "Groups.hpp"

#include "servers/CNShardServer.hpp"

#include "Player.hpp"
#include "PlayerManager.hpp"
#include "Entities.hpp"

/*
 * NOTE: Variadic response packets that list group members are technically
 * double-variadic, as they have two count members with trailing struct counts,
 * and are thus incompatible with the generic sendPacket() wrapper.
 * That means we still have to (carefully) use validOutVarPacket() in this
 * source file.
 */

using namespace Groups;

Group::Group(EntityRef leader) {
    addToGroup(this, leader);
}

static void attachGroupData(std::vector<EntityRef>& pcs, std::vector<EntityRef>& npcs, uint8_t* pivot) {
    for(EntityRef pcRef : pcs) {
        sPCGroupMemberInfo* info = (sPCGroupMemberInfo*)pivot;

        Player* plr = PlayerManager::getPlayer(pcRef.sock);
        info->iPC_ID = plr->iID;
        info->iPCUID = plr->PCStyle.iPC_UID;
        info->iNameCheck = plr->PCStyle.iNameCheck;
        memcpy(info->szFirstName, plr->PCStyle.szFirstName, sizeof(plr->PCStyle.szFirstName));
        memcpy(info->szLastName, plr->PCStyle.szLastName, sizeof(plr->PCStyle.szLastName));
        info->iSpecialState = plr->iSpecialState;
        info->iLv = plr->level;
        info->iHP = plr->HP;
        info->iMaxHP = PC_MAXHEALTH(plr->level);
        // info->iMapType = 0;
        // info->iMapNum = 0;
        info->iX = plr->x;
        info->iY = plr->y;
        info->iZ = plr->z;
        if(plr->activeNano > 0) {
            info->Nano = *plr->getActiveNano();
            info->bNano = true;
        }

        pivot = (uint8_t*)(info + 1);
    }
    for(EntityRef npcRef : npcs) {
        sNPCGroupMemberInfo* info = (sNPCGroupMemberInfo*)pivot;

        // probably should not assume that the combatant is an
        // entity, but it works for now
        BaseNPC* npc = (BaseNPC*)npcRef.getEntity();
        info->iNPC_ID = npcRef.id;
        info->iNPC_Type = npc->type;
        info->iHP = npc->hp;
        info->iX = npc->x;
        info->iY = npc->y;
        info->iZ = npc->z;

        pivot = (uint8_t*)(info + 1);
    }
}

void Groups::addToGroup(Group* group, EntityRef member) {
    if (group == nullptr)
        return;

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

    if(member.kind == EntityKind::PLAYER) {
        std::vector<EntityRef> pcs = group->filter(EntityKind::PLAYER);
        std::vector<EntityRef> npcs = group->filter(EntityKind::COMBAT_NPC);
        size_t pcCount = pcs.size();
        size_t npcCount = npcs.size();

        uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
        memset(respbuf, 0, CN_PACKET_BUFFER_SIZE);
        sP_FE2CL_PC_GROUP_JOIN* pkt = (sP_FE2CL_PC_GROUP_JOIN*)respbuf;

        pkt->iID_NewMember = PlayerManager::getPlayer(member.sock)->iID;
        pkt->iMemberPCCnt = (int32_t)pcCount;
        pkt->iMemberNPCCnt = (int32_t)npcCount;

        if(!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_JOIN), pcCount, sizeof(sPCGroupMemberInfo))
            || !validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_JOIN) + pcCount * sizeof(sPCGroupMemberInfo), npcCount, sizeof(sNPCGroupMemberInfo))) {
            std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_JOIN packet size" << std::endl;
        } else {
            uint8_t* pivot = (uint8_t*)(pkt + 1);
            attachGroupData(pcs, npcs, pivot);
            // PC_GROUP_JOIN_SUCC and PC_GROUP_JOIN carry identical payloads but have different IDs
            // (and the client does care!) so we need to send one to the new member
            // and the other to the rest
            size_t resplen = sizeof(sP_FE2CL_PC_GROUP_JOIN) + pcCount * sizeof(sPCGroupMemberInfo) + npcCount * sizeof(sNPCGroupMemberInfo);
            member.sock->sendPacket(respbuf, P_FE2CL_PC_GROUP_JOIN_SUCC, resplen);
            sendToGroup(group, member, respbuf, P_FE2CL_PC_GROUP_JOIN, resplen);
        }
    }
}

bool Groups::removeFromGroup(Group* group, EntityRef member) {
    if (group == nullptr)
        return false;

    if (member.kind == EntityKind::PLAYER) {
        Player* plr = PlayerManager::getPlayer(member.sock);
        plr->group = nullptr; // no dangling pointers here muahaahahah

        INITSTRUCT(sP_FE2CL_PC_GROUP_LEAVE_SUCC, leavePkt);
        member.sock->sendPacket(leavePkt, P_FE2CL_PC_GROUP_LEAVE_SUCC);
    }
    else if (member.kind == EntityKind::COMBAT_NPC) {
        CombatNPC* npc = (CombatNPC*)member.getEntity();
        npc->group = nullptr;
    }
    else {
        std::cout << "[WARN] Removing a weird entity type from a group" << std::endl;
    }

    auto it = std::find(group->members.begin(), group->members.end(), member);
    if (it == group->members.end()) {
        std::cout << "[WARN] Tried to remove a member that isn't in the group" << std::endl;
    } else {
        group->members.erase(it);
    }

    if(member.kind == EntityKind::PLAYER) {
        std::vector<EntityRef> pcs = group->filter(EntityKind::PLAYER);
        std::vector<EntityRef> npcs = group->filter(EntityKind::COMBAT_NPC);
        size_t pcCount = pcs.size();
        size_t npcCount = npcs.size();

        uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
        memset(respbuf, 0, CN_PACKET_BUFFER_SIZE);
        sP_FE2CL_PC_GROUP_LEAVE* pkt = (sP_FE2CL_PC_GROUP_LEAVE*)respbuf;

        pkt->iID_LeaveMember = PlayerManager::getPlayer(member.sock)->iID;
        pkt->iMemberPCCnt = (int32_t)pcCount;
        pkt->iMemberNPCCnt = (int32_t)npcCount;

        if(!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_LEAVE), pcCount, sizeof(sPCGroupMemberInfo))
            || !validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_LEAVE) + pcCount * sizeof(sPCGroupMemberInfo), npcCount, sizeof(sNPCGroupMemberInfo))) {
            std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_LEAVE packet size" << std::endl;
        } else {
            uint8_t* pivot = (uint8_t*)(pkt + 1);
            attachGroupData(pcs, npcs, pivot);
            sendToGroup(group, respbuf, P_FE2CL_PC_GROUP_LEAVE,
                sizeof(sP_FE2CL_PC_GROUP_LEAVE) + pcCount * sizeof(sPCGroupMemberInfo) + npcCount * sizeof(sNPCGroupMemberInfo));
        }
    }

    if (group->members.size() == 1) {
        return removeFromGroup(group, group->members.back());
    }

    if (group->members.empty()) {
        delete group; // cleanup memory
        return true;
    }
    return false;
}

void Groups::disbandGroup(Group* group) {
    if (group == nullptr)
        return;

    // remove everyone from the group!!
    bool done = false;
    while(!done) {
        EntityRef back = group->members.back();
        done = removeFromGroup(group, back);
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
        return; // disconnect or something

    int size = otherPlr->group == nullptr ? 1 : otherPlr->group->filter(EntityKind::PLAYER).size();

    // fail if the group is full or the other player is already in a group
    if (plr->group != nullptr || size + 1 > 4) {
        INITSTRUCT(sP_FE2CL_PC_GROUP_JOIN_FAIL, resp);
        sock->sendPacket((void*)&resp, P_FE2CL_PC_GROUP_JOIN_FAIL, sizeof(sP_FE2CL_PC_GROUP_JOIN_FAIL));
        return;
    }

    if (otherPlr->group == nullptr) {
        // create group
        EntityRef otherPlrRef = PlayerManager::getSockFromID(recv->iID_From);
        otherPlr->group = new Group(otherPlrRef);
    }
    addToGroup(otherPlr->group, sock);
}

static void leaveGroup(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    groupKick(plr->group, sock);
}

void Groups::sendToGroup(Group* group, void* buf, uint32_t type, size_t size) {
    if (group == nullptr)
        return;

    auto players = group->filter(EntityKind::PLAYER);
    for (EntityRef ref : players) {
        ref.sock->sendPacket(buf, type, size);
    }
}

void Groups::sendToGroup(Group* group, EntityRef excluded, void* buf, uint32_t type, size_t size) {
    if (group == nullptr)
        return;

    auto players = group->filter(EntityKind::PLAYER);
    for (EntityRef ref : players) {
        if(ref != excluded) ref.sock->sendPacket(buf, type, size);
    }
}

void Groups::groupTickInfo(CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    Group* group = plr->group;
    std::vector<EntityRef> pcs = group->filter(EntityKind::PLAYER);
    std::vector<EntityRef> npcs = group->filter(EntityKind::COMBAT_NPC);
    size_t pcCount = pcs.size();
    size_t npcCount = npcs.size();

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, CN_PACKET_BUFFER_SIZE);
    sP_FE2CL_PC_GROUP_MEMBER_INFO* pkt = (sP_FE2CL_PC_GROUP_MEMBER_INFO*)respbuf;

    pkt->iID = plr->iID;
    pkt->iMemberPCCnt = (int32_t)pcCount;
    pkt->iMemberNPCCnt = (int32_t)npcCount;

    if(!validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO), pcCount, sizeof(sPCGroupMemberInfo))
        || !validOutVarPacket(sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) + pcCount * sizeof(sPCGroupMemberInfo), npcCount, sizeof(sNPCGroupMemberInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GROUP_MEMBER_INFO packet size" << std::endl;
    } else {
        uint8_t* pivot = (uint8_t*)(pkt + 1);
        attachGroupData(pcs, npcs, pivot);
        sock->sendPacket(respbuf, P_FE2CL_PC_GROUP_MEMBER_INFO,
            sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) + pcCount * sizeof(sPCGroupMemberInfo) + npcCount * sizeof(sNPCGroupMemberInfo));
    }
}

void Groups::groupKick(Group* group, EntityRef ref) {

    if (group == nullptr)
        return;

    // if you are the group leader, destroy your own group and kick everybody
    if (group->members[0] == ref) {
        disbandGroup(group);
        return;
    }

    removeFromGroup(group, ref);
}

void Groups::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE, requestGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_INVITE_REFUSE, refuseGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_JOIN, joinGroup);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GROUP_LEAVE, leaveGroup);
}
