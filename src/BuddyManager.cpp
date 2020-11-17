#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "BuddyManager.hpp"
#include "Database.hpp"
#include "ItemManager.hpp"
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
    //
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST, emailReceivePageList);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_READ_EMAIL, emailRead);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_CANDY, emailReceiveTaros);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_ITEM, emailReceiveItemSingle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL, emailReceiveItemAll);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_DELETE_EMAIL, emailDelete);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SEND_EMAIL, emailSend);
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
            if (buddyPlayerData.PlayerID == -1)
                continue;
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

    CNSocket* otherSock = nullptr;

    for (auto& pair : PlayerManager::players) {
        Player* plr = pair.second;
        if (strcmp(U16toU8(plr->PCStyle.szFirstName).c_str(), U16toU8(pkt->szFirstName).c_str()) == 0
            && strcmp(U16toU8(plr->PCStyle.szLastName).c_str(), U16toU8(pkt->szLastName).c_str()) == 0
            && !playerHasBuddyWithID(plrReq, plr->iID)) {
            otherSock = pair.first;
            break;
        }
    }

    if (otherSock == nullptr)
        return; // no player found

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

        // add record to db
        Database::addBuddyship(plr->iID, otherPlr->iID);
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
        sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
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
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
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

    // remove record from db
    Database::removeBuddyship(plr->PCStyle.iPC_UID, pkt->iBuddyPCUID);

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

void BuddyManager::emailUpdateCheck(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK))
        return; // malformed packet

    INITSTRUCT(sP_FE2CL_REP_PC_NEW_EMAIL, resp);
    resp.iNewEmailCnt = Database::getUnreadEmailCount(PlayerManager::getPlayer(sock)->iID);
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_NEW_EMAIL, sizeof(sP_FE2CL_REP_PC_NEW_EMAIL));
}

void BuddyManager::emailReceivePageList(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST))
        return; // malformed packet

    sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST* pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST*)data->buf;

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC, resp);
    resp.iPageNum = pkt->iPageNum;

    std::vector<Database::EmailData> emails = Database::getEmails(PlayerManager::getPlayer(sock)->iID, pkt->iPageNum);
    for (int i = 0; i < emails.size(); i++) {
        // convert each email and load them into the packet
        Database::EmailData* email = &emails.at(i);
        sEmailInfo* emailInfo = new sEmailInfo();
        emailInfo->iEmailIndex = email->MsgIndex;
        emailInfo->iReadFlag = email->ReadFlag;
        emailInfo->iItemCandyFlag = email->ItemFlag;
        emailInfo->iFromPCUID = email->SenderId;
        emailInfo->SendTime = timeStampToStruct(email->SendTime);
        emailInfo->DeleteTime = timeStampToStruct(email->DeleteTime);
        U8toU16(email->SenderFirstName, emailInfo->szFirstName, sizeof(emailInfo->szFirstName));
        U8toU16(email->SenderLastName, emailInfo->szLastName, sizeof(emailInfo->szLastName));
        U8toU16(email->SubjectLine, emailInfo->szSubject, sizeof(emailInfo->szSubject));
        resp.aEmailInfo[i] = *emailInfo;
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC, sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC));
}

void BuddyManager::emailRead(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_READ_EMAIL))
        return; // malformed packet

    sP_CL2FE_REQ_PC_READ_EMAIL* pkt = (sP_CL2FE_REQ_PC_READ_EMAIL*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    Database::EmailData email = Database::getEmail(plr->iID, pkt->iEmailIndex);
    sItemBase* attachments = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    email.ReadFlag = 1; // mark as read
    Database::updateEmailContent(&email);

    INITSTRUCT(sP_FE2CL_REP_PC_READ_EMAIL_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;
    resp.iCash = email.Taros;
    for (int i = 0; i < 4; i++) {
        resp.aItem[i] = attachments[i];
    }
    U8toU16(email.MsgBody, (char16_t*)resp.szContent, sizeof(resp.szContent));

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_READ_EMAIL_SUCC, sizeof(sP_FE2CL_REP_PC_READ_EMAIL_SUCC));
}

void BuddyManager::emailReceiveTaros(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY))
        return; // malformed packet

    sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY* pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    Database::EmailData email = Database::getEmail(plr->iID, pkt->iEmailIndex);
    // money transfer
    plr->money += email.Taros;
    email.Taros = 0;
    // update Taros in email
    Database::updateEmailContent(&email);

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC, resp);
    resp.iCandy = plr->money;
    resp.iEmailIndex = pkt->iEmailIndex;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC, sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC));
}

void BuddyManager::emailReceiveItemSingle(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM))
        return; // malformed packet

    sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM* pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // get email item from db and delete it
    sItemBase* attachments = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    sItemBase itemFrom = attachments[pkt->iEmailItemSlot - 1];
    Database::deleteEmailAttachments(plr->iID, pkt->iEmailIndex, pkt->iEmailItemSlot);

    // move item to player inventory
    if (pkt->iSlotNum < 0 || pkt->iSlotNum >= AINVEN_COUNT)
        return; // sanity check
    sItemBase& itemTo = plr->Inven[pkt->iSlotNum];
    itemTo.iID = itemFrom.iID;
    itemTo.iOpt = itemFrom.iOpt;
    itemTo.iTimeLimit = itemFrom.iTimeLimit;
    itemTo.iType = itemFrom.iType;
    
    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;
    resp.iEmailItemSlot = pkt->iEmailItemSlot;
    resp.iSlotNum = pkt->iSlotNum;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC));

    // update inventory
    INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp2);
    resp2.eIL = 1;
    resp2.iSlotNum = resp.iSlotNum;
    resp2.Item = itemTo;

    sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
}

void BuddyManager::emailReceiveItemAll(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL))
        return; // malformed packet

    sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL* pkt = (sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL*)data->buf;

    // move items to player inventory
    Player* plr = PlayerManager::getPlayer(sock);
    sItemBase* itemsFrom = Database::getEmailAttachments(plr->iID, pkt->iEmailIndex);
    for (int i = 0; i < 4; i++) {
        int slot = ItemManager::findFreeSlot(plr);
        if (slot < 0 || slot >= AINVEN_COUNT) {
            INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL, failResp);
            failResp.iEmailIndex = pkt->iEmailIndex;
            failResp.iErrorCode = 0; // ???
            break; // sanity check; should never happen
        }

        // copy data over
        sItemBase itemFrom = itemsFrom[i];
        sItemBase& itemTo = plr->Inven[slot];
        itemTo.iID = itemFrom.iID;
        itemTo.iOpt = itemFrom.iOpt;
        itemTo.iTimeLimit = itemFrom.iTimeLimit;
        itemTo.iType = itemFrom.iType;

        // update inventory
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp2);
        resp2.eIL = 1;
        resp2.iSlotNum = slot;
        resp2.Item = itemTo;

        sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
    }

    // delete all items from db
    Database::deleteEmailAttachments(plr->iID, pkt->iEmailIndex, -1);

    INITSTRUCT(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC, resp);
    resp.iEmailIndex = pkt->iEmailIndex;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC, sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC));
}

void BuddyManager::emailDelete(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_DELETE_EMAIL))
        return; // malformed packet

    sP_CL2FE_REQ_PC_DELETE_EMAIL* pkt = (sP_CL2FE_REQ_PC_DELETE_EMAIL*)data->buf;

    Database::deleteEmails(PlayerManager::getPlayer(sock)->iID, pkt->iEmailIndexArray);

    INITSTRUCT(sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC, resp);
    for (int i = 0; i < 5; i++) {
        resp.iEmailIndexArray[i] = pkt->iEmailIndexArray[i]; // i'm scared of memcpy
    }
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_DELETE_EMAIL_SUCC, sizeof(sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC));
}

void BuddyManager::emailSend(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_SEND_EMAIL))
        return; // malformed packet

    sP_CL2FE_REQ_PC_SEND_EMAIL* pkt = (sP_CL2FE_REQ_PC_SEND_EMAIL*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_PC_SEND_EMAIL_SUCC, resp);

    // handle items
    std::vector<sItemBase> attachments;
    for (int i = 0; i < 4; i++) {
        sEmailItemInfoFromCL attachment = pkt->aItem[i];
        resp.aItem[i] = attachment;
        if (attachment.iSlotNum < 0 || attachment.iSlotNum >= AINVEN_COUNT
            || attachment.ItemInven.iID <= 0 || attachment.ItemInven.iType < 0)
            continue; // sanity check
        attachments.push_back(attachment.ItemInven);
        // delete item
        plr->Inven[attachment.iSlotNum] = { 0, 0, 0, 0 };
    }

    int cost = pkt->iCash + 50 + 20 * attachments.size(); // attached taros + postage
    plr->money -= cost;
    Database::EmailData email = {
        (int)pkt->iTo_PCUID, // PlayerId
        Database::getNextEmailIndex(pkt->iTo_PCUID), // MsgIndex
        0, // ReadFlag (unread)
        (pkt->iCash > 0 || attachments.size() > 0) ? 1 : 0, // ItemFlag
        plr->iID, // SenderID
        U16toU8(plr->PCStyle.szFirstName), // SenderFirstName
        U16toU8(plr->PCStyle.szLastName), // SenderLastName
        ChatManager::sanitizeText(U16toU8(pkt->szSubject)), // SubjectLine
        ChatManager::sanitizeText(U16toU8(pkt->szContent)), // MsgBody
        pkt->iCash, // Taros
        (uint64_t)getTimestamp(), // SendTime
        0 // DeleteTime (unimplemented)
    };

    Database::sendEmail(&email, attachments);

    // HACK: use set value packet to force GUI taros update
    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, tarosResp);
    tarosResp.iPC_ID = plr->iID;
    tarosResp.iSetValueType = 5;
    tarosResp.iSetValue = plr->money;
    sock->sendPacket((void*)&tarosResp, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));

    resp.iCandy = plr->money;
    resp.iTo_PCUID = pkt->iTo_PCUID;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_SEND_EMAIL_SUCC, sizeof(sP_FE2CL_REP_PC_SEND_EMAIL_SUCC));
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

bool BuddyManager::playerHasBuddyWithID(Player* plr, int buddyID) {
    for (int i = 0; i < 50; i++) {
        if (plr->buddyIDs[i] == buddyID)
            return true;
    }
    return false;
}

#pragma endregion
