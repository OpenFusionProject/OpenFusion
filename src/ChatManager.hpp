#pragma once

#include "CNShardServer.hpp"

namespace ChatManager {
    void init();

    void chatHandler(CNSocket* sock, CNPacketData* data);
    void emoteHandler(CNSocket* sock, CNPacketData* data);
    void menuChatHandler(CNSocket* sock, CNPacketData* data);
}
