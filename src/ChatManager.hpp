#pragma once

#include "CNShardServer.hpp"

#define CMD_PREFIX '/'

typedef void (*CommandHandler)(std::string fullString, std::vector<std::string>& args, CNSocket* sock);

struct ChatCommand {
    int requiredAccLevel;
    CommandHandler handlr;

    ChatCommand(int r, CommandHandler h): requiredAccLevel(r), handlr(h) {}
    ChatCommand(): ChatCommand(0, nullptr) {}
};

namespace ChatManager {
    extern std::map<std::string, ChatCommand> commands;
    void init();

    void registerCommand(std::string cmd, int requiredLevel, CommandHandler handlr);

    void chatHandler(CNSocket* sock, CNPacketData* data);
    void emoteHandler(CNSocket* sock, CNPacketData* data);
    void menuChatHandler(CNSocket* sock, CNPacketData* data);
    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD
}
