#include "servers/CNShardServer.hpp"
#include "Buddies.hpp"
#include "PlayerManager.hpp"
#include "Buddies.hpp"
#include "db/Database.hpp"
#include "Items.hpp"
#include "db/Database.hpp"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

using namespace Buddies;

#pragma region Helper methods

static int getAvailableBuddySlot(Player* plr) {
    int slot = -1;
    for (int i = 0; i < 50; i++) {
        if (plr->buddyIDs[i] == 0)
            return i;
    }
    return slot;
}

static bool playerHasBuddyWithID(Player* plr, int buddyID) {
    for (int i = 0; i < 50; i++) {
        if (plr->buddyIDs[i] == buddyID)
            return true;
    }
    return false;
}

#pragma endregion

// Refresh buddy list
void Buddies::refreshBuddyList(CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    int buddyCnt = Database::getNumBuddies(plr);

    if (!validOutVarPacket(sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC), buddyCnt, sizeof(sBuddyBaseInfo))) {
        std::cout << "[WARN] bad sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC) + buddyCnt * sizeof(sBuddyBaseInfo);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC* resp = (sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC*)respbuf;
    sBuddyBaseInfo* respdata = (sBuddyBaseInfo*)(respbuf + sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC));

    // base response fields
    resp->iBuddyCnt = buddyCnt;
    resp->iID = plr->iID;
    resp->iPCUID = plr->PCStyle.iPC_UID;
    resp->iListNum = 0; // ???

    int buddyIndex = 0;
    for (int i = 0; i < 50; i++) {
        int64_t buddyID = plr->buddyIDs[i];
        if (buddyID != 0) {
            sBuddyBaseInfo buddyInfo = {};
            Player buddyPlayerData = {};
            Database::getPlayer(&buddyPlayerData, buddyID);
            if (buddyPlayerData.iID == 0)
                continue;
            buddyInfo.bBlocked = plr->isBuddyBlocked[i];
            buddyInfo.bFreeChat = 1;
            buddyInfo.iGender = buddyPlayerData.PCStyle.iGender;
            buddyInfo.iID = buddyID;
            buddyInfo.iPCUID = buddyID;
            buddyInfo.iNameCheckFlag = buddyPlayerData.PCStyle.iNameCheck;
            buddyInfo.iPCState = buddyPlayerData.iPCState;
            memcpy(buddyInfo.szFirstName, buddyPlayerData.PCStyle.szFirstName, sizeof(buddyInfo.szFirstName));
            memcpy(buddyInfo.szLastName, buddyPlayerData.PCStyle.szLastName, sizeof(buddyInfo.szLastName));
            respdata[buddyIndex] = buddyInfo;
            buddyIndex++;
        }
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC, resplen);
}

// Buddy request
static void requestBuddy(CNSocket* sock, CNPacketData* data) {
    auto req = (sP_CL2FE_REQ_REQUEST_MAKE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(req->iBuddyID);

    if (otherPlr == nullptr)
        return;

    if (getAvailableBuddySlot(plr) == -1 || getAvailableBuddySlot(otherPlr) == -1)
    {
        INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL, failResp);
        sock->sendPacket(failResp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL);
        return;
    }

    CNSocket* otherSock = PlayerManager::getSockFromID(otherPlr->iID);

    INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC, resp);
    INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, otherResp);
    
    resp.iRequestID = plr->iID;
    resp.iBuddyID = req->iBuddyID;
    resp.iBuddyPCUID = req->iBuddyPCUID;

    otherResp.iRequestID = plr->iID;
    otherResp.iBuddyID = req->iBuddyID;
    memcpy(otherResp.szFirstName, plr->PCStyle.szFirstName, sizeof(plr->PCStyle.szFirstName));
    memcpy(otherResp.szLastName, plr->PCStyle.szLastName, sizeof(plr->PCStyle.szLastName));

    std::cout << "Buddy ID: " << req->iBuddyID << std::endl;

    sock->sendPacket(resp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC);
    otherSock->sendPacket(otherResp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER);

}

// Sending buddy request by player name
static void reqBuddyByName(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY*)data->buf;
    Player* plrReq = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromName(AUTOU16TOU8(pkt->szFirstName), AUTOU16TOU8(pkt->szLastName));
    if (otherSock == nullptr)
        return; // no player found

    Player *otherPlr = PlayerManager::getPlayer(otherSock);
    if (playerHasBuddyWithID(plrReq, otherPlr->iID))
        return;

    resp.iPCUID = plrReq->PCStyle.iPC_UID;
    resp.iNameCheckFlag = plrReq->PCStyle.iNameCheck;

    memcpy(resp.szFirstName, plrReq->PCStyle.szFirstName, sizeof(plrReq->PCStyle.szFirstName));
    memcpy(resp.szLastName, plrReq->PCStyle.szLastName, sizeof(plrReq->PCStyle.szLastName));
    otherSock->sendPacket(resp, P_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC);
}

// Accepting buddy request
static void reqAcceptBuddy(CNSocket* sock, CNPacketData* data) {
    auto req = (sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(req->iBuddyID);

    if (otherPlr == nullptr)
        return; // sanity check

    CNSocket* otherSock = PlayerManager::getSockFromID(otherPlr->iID);

    int slotA = getAvailableBuddySlot(plr);
    int slotB = getAvailableBuddySlot(otherPlr);
    if (slotA == -1 || slotB == -1)
        return; // sanity check

    if (req->iAcceptFlag == 1 && plr->iID != otherPlr->iID && !playerHasBuddyWithID(plr, otherPlr->iID))
    {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

        // A to B
        resp.iBuddySlot = slotA;
        resp.BuddyInfo.iID = otherPlr->iID;
        resp.BuddyInfo.iPCUID = otherPlr->PCStyle.iPC_UID;
        resp.BuddyInfo.iPCState = 1; // assumed to be online
        resp.BuddyInfo.bBlocked = 0; // not blocked by default
        resp.BuddyInfo.iGender = otherPlr->PCStyle.iGender; // shows the other player's gender
        resp.BuddyInfo.bFreeChat = 1; // shows whether or not the other player has freechat on (hardcoded for now)
        resp.BuddyInfo.iNameCheckFlag = otherPlr->PCStyle.iNameCheck;
        memcpy(resp.BuddyInfo.szFirstName, otherPlr->PCStyle.szFirstName, sizeof(resp.BuddyInfo.szFirstName));
        memcpy(resp.BuddyInfo.szLastName, otherPlr->PCStyle.szLastName, sizeof(resp.BuddyInfo.szLastName));
        sock->sendPacket(resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC);
        plr->buddyIDs[slotA] = otherPlr->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotA] << std::endl;
        
        // B to A, using the same struct
        resp.iBuddySlot = slotB;
        resp.BuddyInfo.iID = plr->iID;
        resp.BuddyInfo.iPCUID = plr->PCStyle.iPC_UID;
        resp.BuddyInfo.iPCState = 1;
        resp.BuddyInfo.bBlocked = 0;
        resp.BuddyInfo.iGender = plr->PCStyle.iGender;
        resp.BuddyInfo.bFreeChat = 1;
        resp.BuddyInfo.iNameCheckFlag = plr->PCStyle.iNameCheck;
        memcpy(resp.BuddyInfo.szFirstName, plr->PCStyle.szFirstName, sizeof(resp.BuddyInfo.szFirstName));
        memcpy(resp.BuddyInfo.szLastName, plr->PCStyle.szLastName, sizeof(resp.BuddyInfo.szLastName));
        otherSock->sendPacket(resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC);
        otherPlr->buddyIDs[slotB] = plr->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotB] << std::endl;

        // add record to db
        Database::addBuddyship(plr->iID, otherPlr->iID);
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

        declineResp.iErrorCode = 6; // Buddy declined notification
        declineResp.iBuddyID = req->iBuddyID;
        declineResp.iBuddyPCUID = req->iBuddyPCUID;

        otherSock->sendPacket(declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL);
    }

}

// Accepting buddy request from the find name request
static void reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY*)data->buf;

    Player* plrReq = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

    Player* otherPlr = PlayerManager::getPlayerFromID(pkt->iBuddyPCUID);

    if (otherPlr == nullptr)
        return;

    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);

    int slotA = getAvailableBuddySlot(plrReq);
    int slotB = getAvailableBuddySlot(otherPlr);
    if (slotA == -1 || slotB == -1)
        return; // sanity check

    if (pkt->iAcceptFlag == 1 && plrReq->iID != otherPlr->iID && !playerHasBuddyWithID(plrReq, otherPlr->iID)) {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

        // A to B
        resp.iBuddySlot = slotA;
        resp.BuddyInfo.iID = otherPlr->iID;
        resp.BuddyInfo.iPCUID = otherPlr->PCStyle.iPC_UID;
        resp.BuddyInfo.iPCState = 1; // assumed to be online
        resp.BuddyInfo.bBlocked = 0; // not blocked by default
        resp.BuddyInfo.iGender = otherPlr->PCStyle.iGender; // shows the other player's gender
        resp.BuddyInfo.bFreeChat = 1; // shows whether or not the other player has freechat on (hardcoded for now)
        resp.BuddyInfo.iNameCheckFlag = otherPlr->PCStyle.iNameCheck;
        memcpy(resp.BuddyInfo.szFirstName, otherPlr->PCStyle.szFirstName, sizeof(resp.BuddyInfo.szFirstName));
        memcpy(resp.BuddyInfo.szLastName, otherPlr->PCStyle.szLastName, sizeof(resp.BuddyInfo.szLastName));
        sock->sendPacket(resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC);
        plrReq->buddyIDs[slotA] = otherPlr->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotA] << std::endl;

        // B to A, using the same struct
        resp.iBuddySlot = slotB;
        resp.BuddyInfo.iID = plrReq->iID;
        resp.BuddyInfo.iPCUID = plrReq->PCStyle.iPC_UID;
        resp.BuddyInfo.iPCState = 1;
        resp.BuddyInfo.bBlocked = 0;
        resp.BuddyInfo.iGender = plrReq->PCStyle.iGender;
        resp.BuddyInfo.bFreeChat = 1;
        resp.BuddyInfo.iNameCheckFlag = plrReq->PCStyle.iNameCheck;
        memcpy(resp.BuddyInfo.szFirstName, plrReq->PCStyle.szFirstName, sizeof(resp.BuddyInfo.szFirstName));
        memcpy(resp.BuddyInfo.szLastName, plrReq->PCStyle.szLastName, sizeof(resp.BuddyInfo.szLastName));
        otherSock->sendPacket(resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC);
        otherPlr->buddyIDs[slotB] = plrReq->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotB] << std::endl;

        // add record to db
        Database::addBuddyship(plrReq->iID, otherPlr->iID);
    }
    else 
    {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

        declineResp.iErrorCode = 6; // Buddy declined notification
        declineResp.iBuddyPCUID = pkt->iBuddyPCUID;
        otherSock->sendPacket(declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL);
    }

}

// Getting buddy state
static void reqPktGetBuddyState(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);

    /*
     * If the buddy list wasn't synced a second time yet, sync it.
     * Not sure why we have to do it again for the client not to trip up.
     */
    if (!plr->buddiesSynced) {
        refreshBuddyList(sock);
        plr->buddiesSynced = true;
    }
    
    INITSTRUCT(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC, resp);
    
    for (int slot = 0; slot < 50; slot++) {
        resp.aBuddyState[slot] = PlayerManager::getPlayerFromID(plr->buddyIDs[slot]) != nullptr ? 1 : 0;
        resp.aBuddyID[slot] = plr->buddyIDs[slot];
    }

    sock->sendPacket(resp, P_FE2CL_REP_GET_BUDDY_STATE_SUCC);
}

// Blocking the buddy
static void reqBuddyBlock(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_SET_BUDDY_BLOCK*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // sanity checks
    if (pkt->iBuddySlot < 0 || pkt->iBuddySlot >= 50 || plr->buddyIDs[pkt->iBuddySlot] != pkt->iBuddyPCUID)
        return;

    // save in DB
    Database::removeBuddyship(plr->iID, pkt->iBuddyPCUID);
    Database::addBlock(plr->iID, pkt->iBuddyPCUID);

    // save serverside
    // since ID is already in the array, just set it to blocked
    plr->isBuddyBlocked[pkt->iBuddySlot] = true;

    // send response
    INITSTRUCT(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC, resp);
    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;
    sock->sendPacket(resp, P_FE2CL_REP_SET_BUDDY_BLOCK_SUCC);

    // notify the other player he isn't a buddy anymore
    INITSTRUCT(sP_FE2CL_REP_REMOVE_BUDDY_SUCC, otherResp);
    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);
    if (otherSock == nullptr)
        return; // other player isn't online, no broadcast needed
    Player* otherPlr = PlayerManager::getPlayer(otherSock);
    // search for the slot with the requesting player's ID
    otherResp.iBuddyPCUID = plr->PCStyle.iPC_UID;
    for (int i = 0; i < 50; i++) {
        if (otherPlr->buddyIDs[i] == plr->PCStyle.iPC_UID) {
            // remove buddy
            otherPlr->buddyIDs[i] = 0;
            // broadcast
            otherResp.iBuddySlot = i;
            otherSock->sendPacket(otherResp, P_FE2CL_REP_REMOVE_BUDDY_SUCC);
            return;
        }
    }
}

// block non-buddy
static void reqPlayerBlock(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_SET_PC_BLOCK*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    int buddySlot = getAvailableBuddySlot(plr);
    if (buddySlot == -1)
        return;

    // save in DB
    Database::addBlock(plr->iID, pkt->iBlock_PCUID);

    // save serverside
    plr->buddyIDs[buddySlot] = pkt->iBlock_PCUID;
    plr->isBuddyBlocked[buddySlot] = true;

    // send response
    INITSTRUCT(sP_FE2CL_REP_SET_PC_BLOCK_SUCC, resp);
    resp.iBlock_ID = pkt->iBlock_ID;
    resp.iBlock_PCUID = pkt->iBlock_PCUID;
    resp.iBuddySlot = buddySlot;

    sock->sendPacket(resp, P_FE2CL_REP_SET_PC_BLOCK_SUCC);
}

// Deleting the buddy
static void reqBuddyDelete(CNSocket* sock, CNPacketData* data) {
    // note! this packet is used both for removing buddies and blocks
    auto pkt = (sP_CL2FE_REQ_REMOVE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    // remove buddy on our side
    INITSTRUCT(sP_FE2CL_REP_REMOVE_BUDDY_SUCC, resp);
    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;
    if (pkt->iBuddySlot < 0 || pkt->iBuddySlot >= 50 || plr->buddyIDs[pkt->iBuddySlot] != pkt->iBuddyPCUID)
        return; // sanity check

    bool wasBlocked = plr->isBuddyBlocked[resp.iBuddySlot];
    plr->buddyIDs[resp.iBuddySlot] = 0;
    plr->isBuddyBlocked[resp.iBuddySlot] = false;

    sock->sendPacket(resp, P_FE2CL_REP_REMOVE_BUDDY_SUCC);
    
    // remove record from db
    Database::removeBuddyship(plr->PCStyle.iPC_UID, pkt->iBuddyPCUID);
    // try this too
    Database::removeBlock(plr->PCStyle.iPC_UID, pkt->iBuddyPCUID);
    
    if (wasBlocked)
        return;

    // remove buddy on their side, reusing the struct
    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);
    if (otherSock == nullptr)
        return; // other player isn't online, no broadcast needed
    Player* otherPlr = PlayerManager::getPlayer(otherSock);
    // search for the slot with the requesting player's ID
    resp.iBuddyPCUID = plr->PCStyle.iPC_UID;
    for (int i = 0; i < 50; i++) {
        if (otherPlr->buddyIDs[i] == plr->PCStyle.iPC_UID) {
            // remove buddy
            otherPlr->buddyIDs[i] = 0;
            // broadcast
            resp.iBuddySlot = i;
            otherSock->sendPacket(resp, P_FE2CL_REP_REMOVE_BUDDY_SUCC);
            return;
        }
    }
}

// Warping to buddy
static void reqBuddyWarp(CNSocket* sock, CNPacketData* data) {
    Player *plr = PlayerManager::getPlayer(sock);

    auto pkt = (sP_CL2FE_REQ_PC_BUDDY_WARP*)data->buf;

    if (pkt->iSlotNum < 0 || pkt->iSlotNum >= 50)
        return; // sanity check

    Player* otherPlr = PlayerManager::getPlayerFromID(pkt->iBuddyPCUID);
    if (otherPlr == nullptr)
        return; // buddy offline

    // if the player is instanced; no warp allowed
    if (otherPlr->instanceID != INSTANCE_OVERWORLD)
        goto fail;

    // check if the players are at the same point in time (or in the training area or not)
    if (otherPlr->PCStyle2.iPayzoneFlag != plr->PCStyle2.iPayzoneFlag)
        goto fail;

    // do not warp to players on monkeys
    if (otherPlr->onMonkey)
        goto fail;

    // does the player disallow warping?
    if (otherPlr->unwarpable)
        goto fail;

    // otherPlr->instanceID should always be INSTANCE_OVERWORLD at this point
    PlayerManager::sendPlayerTo(sock, otherPlr->x, otherPlr->y, otherPlr->z, otherPlr->instanceID);
    return;

fail:
    INITSTRUCT(sP_FE2CL_REP_PC_BUDDY_WARP_FAIL, resp);
    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iErrorCode = 0;
    sock->sendPacket(resp, P_FE2CL_REP_PC_BUDDY_WARP_FAIL);
}

void Buddies::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REQUEST_MAKE_BUDDY, requestBuddy);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY, reqBuddyByName);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ACCEPT_MAKE_BUDDY, reqAcceptBuddy);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY, reqFindNameBuddyAccept);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_GET_BUDDY_STATE, reqPktGetBuddyState);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SET_BUDDY_BLOCK, reqBuddyBlock);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SET_PC_BLOCK, reqPlayerBlock);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REMOVE_BUDDY, reqBuddyDelete);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BUDDY_WARP, reqBuddyWarp);
}
