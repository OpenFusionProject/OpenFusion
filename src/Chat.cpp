#include "Chat.hpp"
#include "PlayerManager.hpp"
#include "Groups.hpp"
#include "CustomCommands.hpp"

#include <assert.h>

std::vector<std::string> Chat::dump;

using namespace Chat;

static void chatHandler(CNSocket* sock, CNPacketData* data) {
    auto chat = (sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    std::string fullChat = sanitizeText(AUTOU16TOU8(chat->szFreeChat));
    if (fullChat.length() > 1 && fullChat[0] == CMD_PREFIX) { // PREFIX
        CustomCommands::runCmd(fullChat, sock);
        return;
    }

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__MUTE_FREECHAT)
        return;

    std::string logLine = "[FreeChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iPC_ID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    sock->sendPacket(resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC);

    // send to visible players
    PlayerManager::sendToViewable(sock, resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC);
}

static void menuChatHandler(CNSocket* sock, CNPacketData* data) {
    auto chat = (sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    std::string fullChat = sanitizeText(AUTOU16TOU8(chat->szFreeChat));
    std::string logLine = "[MenuChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iPC_ID = PlayerManager::getPlayer(sock)->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    sock->sendPacket(resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC);

    // send to visible players
    PlayerManager::sendToViewable(sock, resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC);
}

static void emoteHandler(CNSocket* sock, CNPacketData* data) {
    auto emote = (sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, resp);
    resp.iEmoteCode = emote->iEmoteCode;
    resp.iID_From = plr->iID;
    sock->sendPacket(resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT);

    // send to visible players (players within render distance)
    PlayerManager::sendToViewable(sock, resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT);
}

void Chat::sendServerMessage(CNSocket* sock, std::string msg) {
    INITSTRUCT(sP_FE2CL_PC_MOTD_LOGIN, motd);

    motd.iType = 1;
    // convert string to u16 and write it to the buffer
    U8toU16(msg, (char16_t*)motd.szSystemMsg, sizeof(motd.szSystemMsg));

    // send the packet :)
    sock->sendPacket(motd, P_FE2CL_PC_MOTD_LOGIN);
}

static void announcementHandler(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (plr->accountLevel > 30)
        return; // only players with account level less than 30 (GM) are allowed to use this command
    auto announcement = (sP_CL2FE_GM_REQ_PC_ANNOUNCE*)data->buf;

    INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
    msg.iAnnounceType = announcement->iAnnounceType;
    msg.iDuringTime = announcement->iDuringTime;
    memcpy(msg.szAnnounceMsg, announcement->szAnnounceMsg, sizeof(msg.szAnnounceMsg));
    std::map<CNSocket*, Player*>::iterator it;

    switch (announcement->iAreaType) {
    case 0: // area (all players in viewable chunks)
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        PlayerManager::sendToViewable(sock, msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        break;
    case 1: // shard
    case 2: // world
        break; // not applicable to OpenFusion
    case 3: // global (all players)
        for (it = PlayerManager::players.begin(); it != PlayerManager::players.end(); it++) {
            CNSocket* allSock = it->first;
            allSock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        }
    default:
        break;
    }

    std::string logLine = "[Bcast " + std::to_string(announcement->iAreaType) + "] " + PlayerManager::getPlayerName(plr, false) + ": " + AUTOU16TOU8(msg.szAnnounceMsg);
    std::cout << logLine << std::endl;
    dump.push_back("**" + logLine + "**");
}

// Buddy freechatting
static void buddyChatHandler(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);

    if (otherSock == nullptr)
        return; // buddy offline

    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;

    std::string fullChat = sanitizeText(AUTOU16TOU8(pkt->szFreeChat));

    if (fullChat.length() > 1 && fullChat[0] == CMD_PREFIX) { // PREFIX
        CustomCommands::runCmd(fullChat, sock);
        return;
    }

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__MUTE_FREECHAT)
        return;

    std::string logLine = "[BuddyChat] " + PlayerManager::getPlayerName(plr) + " (to " + PlayerManager::getPlayerName(otherPlr) + "): " + fullChat;
    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));

    sock->sendPacket(resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC); // confirm send to sender
    otherSock->sendPacket(resp, P_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC); // broadcast send to receiver
}

// Buddy menuchat
static void buddyMenuChatHandler(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC, resp);

    CNSocket* otherSock = PlayerManager::getSockFromID(pkt->iBuddyPCUID);

    if (otherSock == nullptr)
        return; // buddy offline

    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    resp.iFromPCUID = plr->PCStyle.iPC_UID;
    resp.iToPCUID = pkt->iBuddyPCUID;
    resp.iEmoteCode = pkt->iEmoteCode;

    std::string fullChat = sanitizeText(AUTOU16TOU8(pkt->szFreeChat));
    std::string logLine = "[BuddyMenuChat] " + PlayerManager::getPlayerName(plr) + " (to " + PlayerManager::getPlayerName(otherPlr) + "): " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));

    sock->sendPacket(resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC); // confirm send to sender
    otherSock->sendPacket(resp, P_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC); // broadcast send to receiver
}

static void tradeChatHandler(CNSocket* sock, CNPacketData* data) {
    auto pacdat = (sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT*)data->buf;

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT, resp);
    Player *plr = PlayerManager::getPlayer(sock);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    std::string fullChat = sanitizeText(AUTOU16TOU8(pacdat->szFreeChat));
    U8toU16(fullChat, resp.szFreeChat, sizeof(resp.szFreeChat));

    std::string logLine = "[TradeChat] " + PlayerManager::getPlayerName(plr) + " (to " + PlayerManager::getPlayerName(otherPlr) + "): " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    resp.iEmoteCode = pacdat->iEmoteCode;
    sock->sendPacket(resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT);
    otherSock->sendPacket(resp, P_FE2CL_REP_PC_TRADE_EMOTES_CHAT);
}

static void groupChatHandler(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    std::string fullChat = sanitizeText(AUTOU16TOU8(chat->szFreeChat));

    if (fullChat.length() > 1 && fullChat[0] == CMD_PREFIX) { // PREFIX
        CustomCommands::runCmd(fullChat, sock);
        return;
    }

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__MUTE_FREECHAT)
        return;

    std::string logLine = "[GroupChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;
    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iSendPCID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    Groups::sendToGroup(otherPlr, (void*)&resp, P_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC));
}

static void groupMenuChatHandler(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (otherPlr == nullptr)
        return;

    std::string fullChat = sanitizeText(AUTOU16TOU8(chat->szFreeChat));
    std::string logLine = "[GroupMenuChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iSendPCID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    Groups::sendToGroup(otherPlr, (void*)&resp, P_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC));
}

// we only allow plain ascii, at least for now
std::string Chat::sanitizeText(std::string text, bool allowNewlines) {
    int i;
    const int BUFSIZE = 512;
    char buf[BUFSIZE];

    assert(text.size() < BUFSIZE);

    i = 0;
    for (char c : text) {
        if (i >= BUFSIZE-1)
            break;

        if (!allowNewlines && c == '\n')
            continue;

        if ((c >= ' ' && c <= '~') || c == '\n')
            buf[i++] = c;
    }
    buf[i] = 0;

    return std::string(buf);
}

void Chat::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_FREECHAT_MESSAGE, chatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT, emoteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_MENUCHAT_MESSAGE, menuChatHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE, buddyChatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE, buddyMenuChatHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_EMOTES_CHAT, tradeChatHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE, groupChatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE, groupMenuChatHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_ANNOUNCE, announcementHandler);
}
