#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"

void ChatManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_FREECHAT_MESSAGE, chatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT, emoteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_MENUCHAT_MESSAGE, menuChatHandler);
}

void ChatManager::chatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE))
        return; // malformed packet
    sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, resp);
    memcpy(resp.szFreeChat, chat->szFreeChat, sizeof(chat->szFreeChat));
    resp.iPC_ID = plr.plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC));

    // send to visible players
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC));
}

void ChatManager::menuChatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE))
        return; // malformed packet
    sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE*)data->buf;

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, resp);
    memcpy(resp.szFreeChat, chat->szFreeChat, sizeof(chat->szFreeChat));
    resp.iPC_ID = PlayerManager::players[sock].plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC));

    // send to visible players
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC));
}

void ChatManager::emoteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT))
        return; // ignore the malformed packet

    // you can dance with friends!!!!!!!!

    sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT* emote = (sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT*)data->buf;
    PlayerView& plr = PlayerManager::players[sock];

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, resp);
    resp.iEmoteCode = emote->iEmoteCode;
    resp.iID_From = plr.plr->iID;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));

    // send to visible players (players within render distance)
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));
}
