#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "BuddyManager.hpp"
#include "Database.hpp"

#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>

void BuddyManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REQUEST_MAKE_BUDDY, requestBuddy);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY, reqBuddyByName);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_ACCEPT_MAKE_BUDDY, reqAcceptBuddy);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY, reqFindNameBuddyAccept);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE, reqBuddyFreechat);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE, reqBuddyMenuchat);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_GET_BUDDY_STATE, reqPktGetBuddyState);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SET_BUDDY_BLOCK, reqBuddyBlock);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REMOVE_BUDDY, reqBuddyDelete);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_BUDDY_WARP, reqBuddyWarp);
}

// Refresh buddy list
void BuddyManager::refreshBuddyList(CNSocket* sock) {
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
            Database::DbPlayer buddyPlayerData = Database::getDbPlayerById(buddyID);
            buddyInfo.bBlocked = 0;
            buddyInfo.bFreeChat = 1;
            buddyInfo.iGender = buddyPlayerData.Gender;
            buddyInfo.iID = buddyID;
            buddyInfo.iPCUID = buddyID;
            buddyInfo.iNameCheckFlag = buddyPlayerData.NameCheck;
            buddyInfo.iPCState = buddyPlayerData.PCState;
            U8toU16(buddyPlayerData.FirstName, buddyInfo.szFirstName, sizeof(buddyInfo.szFirstName));
            U8toU16(buddyPlayerData.LastName, buddyInfo.szLastName, sizeof(buddyInfo.szLastName));
            respdata[buddyIndex] = buddyInfo;
            buddyIndex++;
        }
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC, resplen);
}

// Buddy request
void BuddyManager::requestBuddy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REQUEST_MAKE_BUDDY))
        return; // malformed packet

    sP_CL2FE_REQ_REQUEST_MAKE_BUDDY* req = (sP_CL2FE_REQ_REQUEST_MAKE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(req->iBuddyID);

    if (getAvailableBuddySlot(plr) == -1 || getAvailableBuddySlot(otherPlr) == -1)
    {
        INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL, failResp);
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL));
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

    sock->sendPacket((void*)&resp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC));
    otherSock->sendPacket((void*)&otherResp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER));

}

// Sending buddy request by player name
void BuddyManager::reqBuddyByName(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY)) {
        return; // malformed packet
    }

    sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY* pkt = (sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY*)data->buf;
    Player* plrReq = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC, resp);

    CNSocket* otherSock = sock;

    int sizeOfRes = sizeof(pkt->szFirstName) / 9; // Maximum size of a player's first name
    int sizeOfLNRes = sizeof(pkt->szLastName) / 17; // Maximum size of a player's last name

    for (auto pair : PlayerManager::players) {
        int sizeOfReq = sizeof(pair.second.plr->PCStyle.szFirstName) / 9;
        int sizeOfLNReq = sizeof(pair.second.plr->PCStyle.szLastName) / 17;
        if (BuddyManager::NameCheck(pair.second.plr->PCStyle.szFirstName, pkt->szFirstName, sizeOfReq, sizeOfRes) == true && BuddyManager::NameCheck(pair.second.plr->PCStyle.szLastName, pkt->szLastName, sizeOfLNReq, sizeOfLNRes) == true) { // This long line of gorgeous parameters is to check if the player's name matches :eyes:
            otherSock = pair.first;
            break;
        }
    }

    resp.iPCUID = plrReq->PCStyle.iPC_UID;
    resp.iNameCheckFlag = plrReq->PCStyle.iNameCheck;

    memcpy(resp.szFirstName, plrReq->PCStyle.szFirstName, sizeof(plrReq->PCStyle.szFirstName));
    memcpy(resp.szLastName, plrReq->PCStyle.szLastName, sizeof(plrReq->PCStyle.szLastName));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC));

}

// Accepting buddy request
void BuddyManager::reqAcceptBuddy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY))
        return; // malformed packet

    sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY* req = (sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(req->iBuddyID);

    CNSocket* otherSock = PlayerManager::getSockFromID(otherPlr->iID);

    int slotA = getAvailableBuddySlot(plr);
    int slotB = getAvailableBuddySlot(otherPlr);
    if (slotA == -1 || slotB == -1)
        return; // sanity check

    if (req->iAcceptFlag == 1 && plr->iID != otherPlr->iID) 
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
        sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
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
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
        otherPlr->buddyIDs[slotB] = plr->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotB] << std::endl;
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

        declineResp.iErrorCode = 6; // Buddy declined notification
        declineResp.iBuddyID = req->iBuddyID;
        declineResp.iBuddyPCUID = req->iBuddyPCUID;

        otherSock->sendPacket((void*)&declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL));
    }

}

// Accepting buddy request from the find name request
void BuddyManager::reqFindNameBuddyAccept(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY)) {
        return; // malformed packet
    }

    sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY* pkt = (sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

    CNSocket* otherSock = sock;

    int sizeOfRes = sizeof(pkt->szFirstName) / 9;
    int sizeOfLNRes = sizeof(pkt->szLastName) / 17;

    for (auto pair : PlayerManager::players) {
        int sizeOfReq = sizeof(pair.second.plr->PCStyle.szFirstName) / 9;
        int sizeOfLNReq = sizeof(pair.second.plr->PCStyle.szLastName) / 17;
        if (BuddyManager::NameCheck(pair.second.plr->PCStyle.szFirstName, pkt->szFirstName, sizeOfReq, sizeOfRes) == true && BuddyManager::NameCheck(pair.second.plr->PCStyle.szLastName, pkt->szLastName, sizeOfLNReq, sizeOfLNRes) == true) {
            otherSock = pair.first;
            break;
        }
    }

    Player* otherPlr = PlayerManager::getPlayer(otherSock);

    int slotA = getAvailableBuddySlot(plr);
    int slotB = getAvailableBuddySlot(otherPlr);
    if (slotA == -1 || slotB == -1)
        return; // sanity check

    if (pkt->iAcceptFlag == 1 && plr->iID != otherPlr->iID) {
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
        sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
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
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
        otherPlr->buddyIDs[slotB] = plr->PCStyle.iPC_UID;
        //std::cout << "Buddy's ID: " << plr->buddyIDs[slotB] << std::endl;
    }
    else 
    {
        INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, declineResp);

        declineResp.iErrorCode = 6; // Buddy declined notification
        declineResp.iBuddyPCUID = pkt->iBuddyPCUID;
        otherSock->sendPacket((void*)&declineResp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL));
    }

}

// Buddy freechatting
void BuddyManager::reqBuddyFreechat(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE* pkt = (sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);

    if (otherSock == nullptr)
        return; // buddy offline

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;

    std::string fullChat = ChatManager::sanitizeText(U16toU8(pkt->szFreeChat));
    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC)); // confirm send to sender
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC)); // broadcast send to receiver
}

// Buddy menuchat
void BuddyManager::reqBuddyMenuchat(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE* pkt = (sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);

    if (otherSock == nullptr)
        return; // buddy offline

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;
    
    std::string fullChat = ChatManager::sanitizeText(U16toU8(pkt->szFreeChat));
    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC)); // confirm send to sender
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC)); // broadcast send to receiver
}

// Getting buddy state
void BuddyManager::reqPktGetBuddyState(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    
    INITSTRUCT(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC, resp);
    
    for (int slot = 0; slot < 50; slot++) {
        resp.aBuddyState[slot] = PlayerManager::getPlayerFromID(plr->buddyIDs[slot]) != nullptr ? 1 : 0;
        resp.aBuddyID[slot] = plr->buddyIDs[slot];
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_GET_BUDDY_STATE_SUCC, sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC));
}

// Blocking the buddy
void BuddyManager::reqBuddyBlock(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SET_BUDDY_BLOCK))
        return; // malformed packet

    sP_CL2FE_REQ_SET_BUDDY_BLOCK* pkt = (sP_CL2FE_REQ_SET_BUDDY_BLOCK*)data->buf;

    INITSTRUCT(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC, resp);

    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;

    // TODO: handle this?

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SET_BUDDY_BLOCK_SUCC, sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC));

}

// Deleting the buddy
void BuddyManager::reqBuddyDelete(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REMOVE_BUDDY))
        return; // malformed packet

    sP_CL2FE_REQ_REMOVE_BUDDY* pkt = (sP_CL2FE_REQ_REMOVE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    // remove buddy on our side
    INITSTRUCT(sP_FE2CL_REP_REMOVE_BUDDY_SUCC, resp);
    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;
    if (pkt->iBuddySlot < 0 || pkt->iBuddySlot >= 50)
        return; // sanity check
    plr->buddyIDs[resp.iBuddySlot] = 0;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_REMOVE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_REMOVE_BUDDY_SUCC));

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
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_REMOVE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_REMOVE_BUDDY_SUCC));
            return;
        }
    }
}

// Warping to buddy
void BuddyManager::reqBuddyWarp(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_BUDDY_WARP))
        return; // malformed packet

    sP_CL2FE_REQ_PC_BUDDY_WARP* pkt = (sP_CL2FE_REQ_PC_BUDDY_WARP*)data->buf;

    if (pkt->iSlotNum < 0 || pkt->iSlotNum >= 50)
        return; // sanity check

    Player* otherPlr = PlayerManager::getPlayerFromID(pkt->iBuddyPCUID);
    if (otherPlr == nullptr)
        return; // buddy offline

    if (otherPlr->instanceID != INSTANCE_OVERWORLD) {
        // player is instanced; no warp allowed
        INITSTRUCT(sP_FE2CL_REP_PC_BUDDY_WARP_FAIL, resp);
        resp.iBuddyPCUID = pkt->iBuddyPCUID;
        resp.iErrorCode = 0;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_BUDDY_WARP_FAIL, sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_FAIL));
        return;
    }

    PlayerManager::sendPlayerTo(sock, otherPlr->x, otherPlr->y, otherPlr->z);
}

#pragma region Helper methods

int BuddyManager::getAvailableBuddySlot(Player* plr) {
    int slot = -1;
    for (int i = 0; i < 50; i++) {
        if (plr->buddyIDs[i] == 0)
            return i;
    }
    return slot;
}

bool BuddyManager::NameCheck(char16_t reqName[], char16_t resName[], int sizeOfLNReq, int sizeOfLNRes) {
    // If lengths of array are not equal means
    // array are not equal
    if (sizeOfLNReq != sizeOfLNRes)
        return false;

    // Sort both arrays
    std::sort(reqName, reqName + sizeOfLNReq);
    std::sort(resName, resName + sizeOfLNRes);

    // Linearly compare elements
    for (int i = 0; i < sizeOfLNReq; i++)
        if (reqName[i] != resName[i])
            return false;

    // If all elements were same.
    return true;
}

#pragma endregion
