#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"

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
    if (chat->szFreeChat[0] == '/')
    {
        std::string text = "";
        std::string temp = "";
        std::string mapNum = "";
        int space = 0;
        for (int x = 0; x < 16; x++)
        {

            if (chat->szFreeChat[x] == ' ')
                space += 1;
            if (space == 0)
            {
                temp = chat->szFreeChat[x];
                text += temp;
            }
            if (space == 1 && chat->szFreeChat[x] != ' ')
            {
                temp = chat->szFreeChat[x];
                mapNum += temp;
            }

        }
        if (text == "/sendToMap" && std::stoi(mapNum) > -1)
        {
            if (plr.plr == nullptr)
                return;
            NPCManager::changeNPCMAP(sock, PlayerManager::players[sock], std::stoi(mapNum));
            std::cout << text << std::endl;
            std::cout << mapNum << std::endl;
        }
        else if (text == "/summonW" && std::stoi(mapNum) > -1)
        {
            if (plr.plr == nullptr)
                return;

            NPCManager::SummonWrite(sock, PlayerManager::players[sock], std::stoi(mapNum));
            std::cout << text << std::endl;

            std::cout << "Summoned" << mapNum << std::endl;
        }
        else if (text.find("/unSummonW") != std::string::npos)
        {
            if (plr.plr == nullptr)
                return;
            NPCManager::unSummonWrite(sock, PlayerManager::players[sock]);
            std::cout << text << std::endl;
            std::cout << "Unsummoned" << std::endl;
        }
        else if (text == "/tableWrite")
        {
            if (plr.plr == nullptr)
                return;
            NPCManager::tableWrite(sock, PlayerManager::players[sock], std::stoi(mapNum));
            std::cout << text << std::endl;
            std::cout << mapNum << std::endl;
            std::cout << "Wrote to Json" << std::endl;
        }
        else if (text.find("/clearChanges") != std::string::npos)
        {
            if (plr.plr == nullptr)
                return;
            NPCManager::clearChanges(sock, PlayerManager::players[sock]);
            std::cout << text << std::endl;
            std::cout << "changesCleared" << std::endl;
        }
        return;
    }
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
