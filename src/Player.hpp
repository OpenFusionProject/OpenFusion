#pragma once

#include <string>
#include <cstring>

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

struct Player {
    int accountId;
    int64_t SerialKey;
    int32_t iID;
    uint64_t FEKey;

    int level;
    int HP;
    int slot; // player slot, not nano slot
    int32_t money;
    int32_t fusionmatter;
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;
    sNano Nanos[37]; // acquired nanos
    int equippedNanos[3];
    int activeNano; // active nano (index into Nanos)
    int8_t iPCState;

    int x, y, z, angle;
    sItemBase Equip[AEQUIP_COUNT];
    sItemBase Inven[AINVEN_COUNT];
    sItemTrade Trade[12];
    int32_t moneyInTrade;
    bool isTrading;
    bool isTradeConfirm;
    bool IsGM;
};
