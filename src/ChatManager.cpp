#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"

#include <sstream>
#include <iterator>

std::map<std::string, ChatCommand> ChatManager::commands;

std::vector<std::string> parseArgs(std::string full) {
    std::stringstream ss(full);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    return std::vector<std::string>(begin, end);
}

bool runCmd(std::string full, CNSocket* sock) {
    std::vector<std::string> args = parseArgs(full);
    std::string cmd = args[0].substr(1, args[0].size() - 1);
    
    // check if the command exists
    if (ChatManager::commands.find(cmd) != ChatManager::commands.end()) {
        Player* plr = PlayerManager::getPlayer(sock);
        ChatCommand command = ChatManager::commands[cmd];
        
        // sanity check + does the player have the required account level to use the command?
        if (plr != nullptr && plr->accountLevel <= command.requiredAccLevel) {
            command.handlr(full, args, sock);
            return true;
        } else {
            ChatManager::sendServerMessage(sock, "You don't have access to that command!");
            return false;
        }
    }

    ChatManager::sendServerMessage(sock, "Unknown command!");
    return false;
}

void testCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "Test command is working! Here are your passed args:");

    for (std::string arg : args) {
        ChatManager::sendServerMessage(sock, arg);
    }
}

void accessCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "Your access level is " + std::to_string(PlayerManager::getPlayer(sock)->accountLevel));
}

void ChatManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_FREECHAT_MESSAGE, chatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT, emoteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_MENUCHAT_MESSAGE, menuChatHandler);

    registerCommand("test", 1, testCommand);
    registerCommand("access", 100, accessCommand);
    // TODO: add help command
}

void ChatManager::registerCommand(std::string cmd, int requiredLevel, CommandHandler handlr) {
    commands[cmd] = ChatCommand(requiredLevel, handlr);
}

void ChatManager::chatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE))
        return; // malformed packet
    
    sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    std::string fullChat = U16toU8(chat->szFreeChat);

    if (fullChat.length() > 1 && fullChat[0] == CMD_PREFIX) { // PREFIX
        runCmd(fullChat, sock);
        return;
    }

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, resp);
    memcpy(resp.szFreeChat, chat->szFreeChat, sizeof(chat->szFreeChat));
    resp.iPC_ID = plr->iID;
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
    Player* plr = PlayerManager::getPlayer(sock);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, resp);
    resp.iEmoteCode = emote->iEmoteCode;
    resp.iID_From = plr->iID;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));

    // send to visible players (players within render distance)
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));
}

void ChatManager::sendServerMessage(CNSocket* sock, std::string msg) {
    INITSTRUCT(sP_FE2CL_PC_MOTD_LOGIN, motd);

    motd.iType = 1;
    // convert string to u16 and write it to the buffer (TODO: add sanity check to prevent buffer overflow)
    U8toU16(msg, (char16_t*)motd.szSystemMsg);

    // send the packet :)
    sock->sendPacket((void*)&motd, P_FE2CL_PC_MOTD_LOGIN, sizeof(sP_FE2CL_PC_MOTD_LOGIN));
}
