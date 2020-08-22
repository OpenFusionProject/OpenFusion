#ifndef _CM_HPP
#define _CM_HPP

#include "CNShardServer.hpp"

namespace ChatManager {
    void init();

    void chatHandler(CNSocket* sock, CNPacketData* data);
    void emoteHandler(CNSocket* sock, CNPacketData* data);
    void menuChatHandler(CNSocket* sock, CNPacketData* data);
}

#endif
