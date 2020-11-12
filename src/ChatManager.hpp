#pragma once

#include "CNShardServer.hpp"

#define CMD_PREFIX '/'
#define M_PI 3.14159265358979323846

typedef void (*CommandHandler)(std::string fullString, std::vector<std::string>& args, CNSocket* sock);

struct ChatCommand {
    int requiredAccLevel;
    std::string help;
    CommandHandler handlr;

    ChatCommand(int r, CommandHandler h): requiredAccLevel(r), handlr(h) {}
    ChatCommand(int r, CommandHandler h, std::string str): requiredAccLevel(r), help(str), handlr(h) {}
    ChatCommand(): ChatCommand(0, nullptr) {}
};

namespace ChatManager {
    extern std::map<std::string, ChatCommand> commands;
    void init();

    void registerCommand(std::string cmd, int requiredLevel, CommandHandler handlr, std::string help = "");

    void chatHandler(CNSocket* sock, CNPacketData* data);
    void emoteHandler(CNSocket* sock, CNPacketData* data);
    void menuChatHandler(CNSocket* sock, CNPacketData* data);
    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD

    std::string sanitizeText(std::string text);
}
