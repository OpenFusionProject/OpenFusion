#pragma once

#define CMD_PREFIX '/'

#include "CNShardServer.hpp"

namespace ChatManager {
    extern std::vector<std::string> dump;
    void init();

    void chatHandler(CNSocket* sock, CNPacketData* data);
    void emoteHandler(CNSocket* sock, CNPacketData* data);
    void menuChatHandler(CNSocket* sock, CNPacketData* data);
    void announcementHandler(CNSocket* sock, CNPacketData* data);

    void buddyChatHandler(CNSocket* sock, CNPacketData* data);
    void buddyMenuChatHandler(CNSocket* sock, CNPacketData* data);

    void tradeChatHandler(CNSocket* sock, CNPacketData* data);

    void groupChatHandler(CNSocket* sock, CNPacketData* data);
    void groupMenuChatHandler(CNSocket* sock, CNPacketData* data);

    void sendServerMessage(CNSocket* sock, std::string msg); // uses MOTD
    std::string sanitizeText(std::string text, bool allowNewlines=false);
}
