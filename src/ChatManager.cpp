#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "TransportManager.hpp"
#include "TableData.hpp"
#include "limits.h"

#include <sstream>
#include <iterator>
#include <cmath>

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

void populationCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, std::to_string(PlayerManager::players.size()) + " players online");
}

void levelCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);
    if (plr == nullptr)
        return;

    char *tmp;
    int level = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if ((level < 1 || level > 36) && plr->accountLevel > 30)
        return;

    if (!(level < 1 || level > 36))
        plr->level = level;

    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_LEVEL, resp);

    resp.iPC_ID = plr->iID;
    resp.iPC_Level = level;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
}

void mssCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "[MSS] Too few arguments");
        ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test/export> <<height>>");
        return;
    }

    // Validate route number
    char* routeNumC;
    int routeNum = std::strtol(args[1].c_str(), &routeNumC, 10);
    if (*routeNumC) {
        // not an integer
        ChatManager::sendServerMessage(sock, "[MSS] Invalid route number '" + args[1] + "'");
        return;
    }

    if (args.size() < 3) {
        ChatManager::sendServerMessage(sock, "[MSS] Too few arguments");
        ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test> <<height>>");
        return;
    }

    // get the route (if it doesn't exist yet, this will also make it)
    std::vector<WarpLocation>* route = &TableData::RunningSkywayRoutes[routeNum];

    // mss <route> add <height>
    if (args[2] == "add") {
        // make sure height token exists
        if (args.size() < 4) {
            ChatManager::sendServerMessage(sock, "[MSS] Point height must be specified");
            ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> add <height>");
            return;
        }
        // validate height token
        char* heightC;
        int height = std::strtol(args[3].c_str(), &heightC, 10);
        if (*heightC) {
            ChatManager::sendServerMessage(sock, "[MSS] Invalid height " + args[3]);
            return;
        }

        Player* plr = PlayerManager::getPlayer(sock);
        route->push_back({ plr->x, plr->y, height }); // add point
        ChatManager::sendServerMessage(sock, "[MSS] Added point (" + std::to_string(plr->x) + ", " + std::to_string(plr->y) + ", " + std::to_string(height) + ") to route " + std::to_string(routeNum));
        return;
    }

    // mss <route> remove
    if (args[2] == "remove") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->back();
        route->pop_back(); // remove point at top of stack
        ChatManager::sendServerMessage(sock, "[MSS] Removed point (" + std::to_string(pulled.x) + ", " + std::to_string(pulled.y) + ", " + std::to_string(pulled.z) + ") from route " + std::to_string(routeNum));
        return;
    }

    // mss <route> goto
    if (args[2] == "goto") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->back();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        return;
    }

    // mss <route> clear
    if (args[2] == "clear") {
        route->clear();
        ChatManager::sendServerMessage(sock, "[MSS] Cleared route " + std::to_string(routeNum));
        return;
    }

    // mss <route> test
    if (args[2] == "test") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->front();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        TransportManager::testMssRoute(sock, route);
        return;
    }

    // for compatibility: mss <route> export
    if (args[2] == "export") {
        ChatManager::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
        TableData::flush();
        return;
    }

    // mss ????
    ChatManager::sendServerMessage(sock, "[MSS] Unknown command '" + args[2] + "'");

}

void npcRotateCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {

    PlayerView& plrv = PlayerManager::players[sock];
    Player* plr = plrv.plr;
    
    BaseNPC* npc = nullptr;
    int lastDist = INT_MAX;
    for (auto c = plrv.currentChunks.begin(); c != plrv.currentChunks.end(); c++) { // haha get it
        Chunk* chunk = *c;
        for (auto _npc = chunk->NPCs.begin(); _npc != chunk->NPCs.end(); _npc++) {
            BaseNPC* npcTemp = NPCManager::NPCs[*_npc];
            int distXY = std::hypot(plr->x - npcTemp->appearanceData.iX, plr->y - npcTemp->appearanceData.iY);
            int dist = std::hypot(distXY, plr->z - npcTemp->appearanceData.iZ);
            if (dist < lastDist) {
                npc = npcTemp;
                lastDist = dist;
            }
        }
    }

    if (npc == nullptr) {
        ChatManager::sendServerMessage(sock, "[NPCR] No NPCs found nearby");
        return;
    }

    int angle = (plr->angle + 180) % 360;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ, angle);
    TableData::RunningNPCRotations[npc->appearanceData.iNPC_ID] = angle;
    
    // update rotation clientside
    INITSTRUCT(sP_FE2CL_NPC_ENTER, pkt);
    pkt.NPCAppearanceData = npc->appearanceData;
    sock->sendPacket((void*)&pkt, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));

    ChatManager::sendServerMessage(sock, "[NPCR] Successfully set angle to " + std::to_string(angle) + " for NPC " + std::to_string(npc->appearanceData.iNPC_ID));
}

void refreshCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    PlayerManager::sendPlayerTo(sock, plr->x, plr->y, plr->z);
}

void flushCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
    TableData::flush();
}

void ChatManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_FREECHAT_MESSAGE, chatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT, emoteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_MENUCHAT_MESSAGE, menuChatHandler);

    registerCommand("test", 1, testCommand);
    registerCommand("access", 100, accessCommand);
    // TODO: add help command
    registerCommand("mss", 30, mssCommand);
    registerCommand("npcr", 30, npcRotateCommand);
    registerCommand("flush", 30, flushCommand);
    registerCommand("level", 50, levelCommand);
    registerCommand("population", 100, populationCommand);
    registerCommand("refresh", 100, refreshCommand);
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
    // convert string to u16 and write it to the buffer
    U8toU16(msg, (char16_t*)motd.szSystemMsg, sizeof(motd.szSystemMsg));

    // send the packet :)
    sock->sendPacket((void*)&motd, P_FE2CL_PC_MOTD_LOGIN, sizeof(sP_FE2CL_PC_MOTD_LOGIN));
}
