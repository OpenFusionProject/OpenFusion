#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "BuddyManager.hpp"

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

// Buddy request
void BuddyManager::requestBuddy(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REQUEST_MAKE_BUDDY))
        return; // malformed packet

    sP_CL2FE_REQ_REQUEST_MAKE_BUDDY* req = (sP_CL2FE_REQ_REQUEST_MAKE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(req->iBuddyID);

    if (plr == nullptr || otherPlr == nullptr)
        return;

    if (plr->buddyCnt >= 50 || otherPlr->buddyCnt >= 50)
    {
        INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL, failResp);
        sock->sendPacket((void*)&failResp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL));
        return;
    }

    CNSocket* otherSock = PlayerManager::getSockFromID(otherPlr->iID);

    if (otherSock == nullptr)
        return;

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

    if (plrReq == nullptr)
        return;

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

    if (plr == nullptr || otherPlr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromID(otherPlr->iID);

    resp.iBuddySlot = plr->buddyCnt;
    resp.BuddyInfo.iID = otherPlr->iID;
    resp.BuddyInfo.iPCUID = otherPlr->PCStyle.iPC_UID;
    resp.BuddyInfo.iPCState = 1; //to show whether the other player is online or offline (hardcoded for now)
    resp.BuddyInfo.bBlocked = 0; //to show whether the other player is blocked (hardcoded for now)
    resp.BuddyInfo.iGender = otherPlr->PCStyle.iGender; //shows the other player's gender
    resp.BuddyInfo.bFreeChat = 1; //shows whether or not the other player has freechat on (hardcoded for now)
    resp.BuddyInfo.iNameCheckFlag = otherPlr->PCStyle.iNameCheck;
    memcpy(resp.BuddyInfo.szFirstName, otherPlr->PCStyle.szFirstName, sizeof(otherPlr->PCStyle.szFirstName));
    memcpy(resp.BuddyInfo.szLastName, otherPlr->PCStyle.szLastName, sizeof(otherPlr->PCStyle.szLastName));

    if (req->iAcceptFlag == 1) 
    {
        sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
        plr->buddyIDs[plr->buddyCnt] = otherPlr->PCStyle.iPC_UID;
        std::cout << "Buddy's ID: " << plr->buddyIDs[plr->buddyCnt] << std::endl;
        plr->buddyCnt++;


        if (plr->iID != otherPlr->iID) 
        {
            resp.iBuddySlot = otherPlr->buddyCnt;
            resp.BuddyInfo.iID = plr->iID;
            resp.BuddyInfo.iPCUID = plr->PCStyle.iPC_UID;
            resp.BuddyInfo.iPCState = 1; //to show whether the other player is online or offline (hardcoded for now)
            resp.BuddyInfo.bBlocked = 0; //to show whether the other player is blocked (hardcoded for now)
            resp.BuddyInfo.iGender = plr->PCStyle.iGender; //shows the other player's gender
            resp.BuddyInfo.bFreeChat = 1; //shows whether or not the other player has freechat on (hardcoded for now)
            resp.BuddyInfo.iNameCheckFlag = plr->PCStyle.iNameCheck;
            memcpy(resp.BuddyInfo.szFirstName, plr->PCStyle.szFirstName, sizeof(plr->PCStyle.szFirstName));
            memcpy(resp.BuddyInfo.szLastName, plr->PCStyle.szLastName, sizeof(plr->PCStyle.szLastName));
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
            otherPlr->buddyIDs[otherPlr->buddyCnt] = plr->PCStyle.iPC_UID;
            std::cout << "Buddy's ID: " << plr->buddyIDs[plr->buddyCnt] << std::endl;
            otherPlr->buddyCnt++;
        }
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

    if (plrReq == nullptr)
        return;

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

    PlayerView& plr = PlayerManager::players[otherSock];


    resp.iBuddySlot = plrReq->buddyCnt;
    resp.BuddyInfo.iID = plr.plr->iID;
    resp.BuddyInfo.iPCUID = pkt->iBuddyPCUID;
    resp.BuddyInfo.iNameCheckFlag = plr.plr->PCStyle.iNameCheck;
    resp.BuddyInfo.iPCState = 1;
    resp.BuddyInfo.iGender = plr.plr->PCStyle.iGender;
    resp.BuddyInfo.bBlocked = 0;
    resp.BuddyInfo.bFreeChat = 1;

    memcpy(resp.BuddyInfo.szFirstName, plr.plr->PCStyle.szFirstName, sizeof(plr.plr->PCStyle.szFirstName));
    memcpy(resp.BuddyInfo.szLastName, plr.plr->PCStyle.szLastName, sizeof(plr.plr->PCStyle.szLastName));

    if (pkt->iAcceptFlag == 1) 
    {

        sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
        plrReq->buddyIDs[plrReq->buddyCnt] = plr.plr->PCStyle.iPC_UID;
        plrReq->buddyCnt++;

        if (plr.plr->PCStyle.iPC_UID == pkt->iBuddyPCUID) 
        {
            resp.iBuddySlot = plr.plr->buddyCnt;
            resp.BuddyInfo.iID = plrReq->iID;
            resp.BuddyInfo.iPCUID = plrReq->PCStyle.iPC_UID;
            resp.BuddyInfo.iNameCheckFlag = plrReq->PCStyle.iNameCheck;
            resp.BuddyInfo.iPCState = 1;
            resp.BuddyInfo.iGender = plrReq->PCStyle.iGender;
            resp.BuddyInfo.bBlocked = 0;
            resp.BuddyInfo.bFreeChat = 1;

            memcpy(resp.BuddyInfo.szFirstName, plrReq->PCStyle.szFirstName, sizeof(plrReq->PCStyle.szFirstName));
            memcpy(resp.BuddyInfo.szLastName, plrReq->PCStyle.szLastName, sizeof(plrReq->PCStyle.szLastName));

            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
            plr.plr->buddyIDs[plr.plr->buddyCnt] = plrReq->PCStyle.iPC_UID;
            plr.plr->buddyCnt++;
        
        }
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

    if (plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = sock;

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;
    memcpy(resp.szFreeChat, pkt->szFreeChat, sizeof(pkt->szFreeChat));
    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC)); // shows the player that they sent the message to their buddy

    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->PCStyle.iPC_UID != plr->PCStyle.iPC_UID) {
            otherSock = pair.first;
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC)); // sends the message to the buddy.
        }
    }


}

// Buddy menuchat
void BuddyManager::reqBuddyMenuchat(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE* pkt = (sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = sock;

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;
    memcpy(resp.szFreeChat, pkt->szFreeChat, sizeof(pkt->szFreeChat));
    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC)); // shows the player that they sent the message to their buddy

    for (auto pair : PlayerManager::players) {
        if (pair.second.plr->PCStyle.iPC_UID != plr->PCStyle.iPC_UID) {
            otherSock = pair.first;
            otherSock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC)); // sends the message to the buddy.
        }
    }
}

// Getting buddy state
void BuddyManager::reqPktGetBuddyState(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    
    INITSTRUCT(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC, resp);
    
    for (int BuddySlot = 0; BuddySlot < 50; BuddySlot++) {
        //resp.aBuddyState[plr->buddyCnt] = 1; // this sets every buddy to online. Will get the pcstate right directly from the DB.
        resp.aBuddyID[BuddySlot] = plr->buddyIDs[BuddySlot];
        sock->sendPacket((void*)&resp, P_FE2CL_REP_GET_BUDDY_STATE_SUCC, sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC));
    }
}

// Blocking the buddy
void BuddyManager::reqBuddyBlock(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SET_BUDDY_BLOCK))
        return; // malformed packet

    sP_CL2FE_REQ_SET_BUDDY_BLOCK* pkt = (sP_CL2FE_REQ_SET_BUDDY_BLOCK*)data->buf;

    INITSTRUCT(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC, resp);

    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SET_BUDDY_BLOCK_SUCC, sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC));

}

// Deleting the buddy
void BuddyManager::reqBuddyDelete(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REMOVE_BUDDY))
        return; // malformed packet

    sP_CL2FE_REQ_REMOVE_BUDDY* pkt = (sP_CL2FE_REQ_REMOVE_BUDDY*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_REMOVE_BUDDY_SUCC, resp);

    resp.iBuddyPCUID = pkt->iBuddyPCUID;
    resp.iBuddySlot = pkt->iBuddySlot;

    plr->buddyIDs[resp.iBuddySlot] -= resp.iBuddyPCUID;
    plr->buddyCnt--;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_REMOVE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_REMOVE_BUDDY_SUCC));
}

// Warping to buddy
void BuddyManager::reqBuddyWarp(CNSocket* sock, CNPacketData* data) {} // stub

/*#pragma region Helper methods

void BuddyManager::requestedBuddy(CNSocket* sock, Player* plrReq, PlayerView& plr) {
    INITSTRUCT(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, resp);

    resp.iRequestID = plrReq->iID;
    resp.iBuddyID = plr.plr->iID;

    memcpy(resp.szFirstName, plrReq->PCStyle.szFirstName, sizeof(plrReq->PCStyle.szFirstName));
    memcpy(resp.szLastName, plrReq->PCStyle.szLastName, sizeof(plrReq->PCStyle.szLastName));

    sock->sendPacket((void*)&resp, P_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER, sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER)); // player get the buddy request.

}

// Buddy list load
void BuddyManager::buddyList(CNSocket* sock, sBuddyBaseInfo BuddyInfo) {

    size_t resplen = sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC) + sizeof(sBuddyBaseInfo);
    uint8_t respbuf[4096];

    memset(respbuf, 0, resplen);

    sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC* resp = (sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC*)respbuf;
    sBuddyBaseInfo* respdata = (sBuddyBaseInfo*)(respbuf + sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC));

    resp->iID = BuddyInfo.iID;
    resp->iPCUID = BuddyInfo.iPCUID;

    for (int i = 0; i < resp->iBuddyCnt && i < resp->iListNum; i++) {
        respdata->iID = BuddyInfo.iID;
        respdata->iPCUID = BuddyInfo.iPCUID;
        respdata->iGender = BuddyInfo.iGender;
        respdata->iPCState = BuddyInfo.iPCState;
        respdata->iNameCheckFlag = BuddyInfo.iNameCheckFlag;
        respdata->bBlocked = 0;
        respdata->bFreeChat = 1;
        memcpy(respdata->szFirstName, BuddyInfo.szFirstName, sizeof(BuddyInfo.szFirstName));
        memcpy(respdata->szLastName, BuddyInfo.szLastName, sizeof(BuddyInfo.szLastName));
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC, resplen); // updates/loads player's buddy list

}

// If the requested player accepts the buddy request, the requester's buddylist will get loaded up.
void BuddyManager::otherAcceptBuddy(CNSocket* sock, int32_t BuddyID, int64_t BuddyPCUID, sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC resp, Player* plr) {

    // resp.iBuddySlot = 0; //hard-coding this for now
    resp.BuddyInfo.iID = BuddyID;
    resp.BuddyInfo.iPCUID = BuddyPCUID;
    resp.BuddyInfo.iNameCheckFlag = plr->PCStyle.iNameCheck;
    resp.BuddyInfo.iPCState = plr->iPCState;
    resp.BuddyInfo.iGender = plr->PCStyle.iGender;
    resp.BuddyInfo.bBlocked = 0;
    resp.BuddyInfo.bFreeChat = 1;

    memcpy(resp.BuddyInfo.szFirstName, plr->PCStyle.szFirstName, sizeof(plr->PCStyle.szFirstName));
    memcpy(resp.BuddyInfo.szLastName, plr->PCStyle.szLastName, sizeof(plr->PCStyle.szLastName));

    sock->sendPacket((void*)&resp, P_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC, sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC));
    buddyList(sock, resp.BuddyInfo);
}

// Check if the requested name matches the requested player's name
bool BuddyManager::firstNameCheck(char16_t reqFirstName[], char16_t resFirstName[], int sizeOfReq, int sizeOfRes) {
    // If lengths of array are not equal means
    // array are not equal
    if (sizeOfReq != sizeOfRes)
        return false;

    // Sort both arrays
    std::sort(reqFirstName, reqFirstName + sizeOfReq);
    std::sort(resFirstName, resFirstName + sizeOfRes);

    // Linearly compare elements
    for (int i = 0; i < sizeOfReq; i++)
        if (reqFirstName[i] != resFirstName[i])
            return false;

    // If all elements were same.
    return true;
}*/

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
