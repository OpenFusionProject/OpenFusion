#ifndef _MM_HPP
#define _MM_HPP

#include "CNShardServer.hpp"

namespace MissionManager {
    void init();

    void acceptMission(CNSocket* sock, CNPacketData* data);
    void completeMission(CNSocket* sock, CNPacketData* data);
    void setMission(CNSocket* sock, CNPacketData* data);
    void quitMission(CNSocket* sock, CNPacketData* data);
}

#endif