#pragma once

#include "CNShardServer.hpp"

namespace ItemManager {
    void init();	
    void itemMoveHandler(CNSocket* sock, CNPacketData* data);   
    void itemDeleteHandler(CNSocket* sock, CNPacketData* data);   
    void itemGMGiveHandler(CNSocket* sock, CNPacketData* data);
}
