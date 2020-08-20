#ifndef _NM_HPP
#define _NM_HPP

#include "CNShardServer.hpp"

namespace NanoManager {
    void init();
    void nanoSummonHandler(CNSocket* sock, CNPacketData* data);
}

#endif