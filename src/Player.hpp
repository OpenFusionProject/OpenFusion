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
    int slot; // player slot, not nano slot
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;
    sNano Nanos[37]; // acquired nanos
    int equippedNanos[3];
    int activeNano; // active nano (index into Nanos)

    int x, y, z, angle;
    sItemBase Equip[AEQUIP_COUNT];
    sItemBase Inven[AINVEN_COUNT];
    bool IsGM;
};

#endif
