#pragma once

#include <string>
#include <cstring>

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

#define ACTIVE_MISSION_COUNT 6

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
    int32_t batteryW;
    int32_t batteryN;
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;
    sNano Nanos[37]; // acquired nanos
    int equippedNanos[3];
    int activeNano; // active nano (index into Nanos)
    int8_t iPCState;
    int32_t iWarpLocationFlag;
    int64_t aSkywayLocationFlag[2];

    int x, y, z, angle;
    sItemBase Equip[AEQUIP_COUNT];
    sItemBase Inven[AINVEN_COUNT];
    sItemBase Bank[ABANK_COUNT];
    sItemTrade Trade[12];
    int32_t moneyInTrade;
    bool isTrading;
    bool isTradeConfirm;
    bool IsGM;

    int64_t aQuestFlag[16];
    int tasks[ACTIVE_MISSION_COUNT];
    sItemBase QInven[AQINVEN_COUNT];
};
