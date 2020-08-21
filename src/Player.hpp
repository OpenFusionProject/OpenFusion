#include <string>
#include <cstring>

#ifndef _PLR_HPP
#define _PLR_HPP

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

struct Player {
    int64_t SerialKey;
    int32_t iID;
    uint64_t FEKey;

    int level;
    int HP;
    int slot;
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;
    sNano Nanos[37];

    int x, y, z, angle;
    sItemBase Equip[AEQUIP_COUNT];
};

#endif