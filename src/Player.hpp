#pragma once

#include <string>
#include <cstring>

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

#define ACTIVE_MISSION_COUNT 6

#define PC_MAXHEALTH(level) (925 + 75 * (level))

struct Player {
    int accountId;
    int accountLevel; // permission level (see CN_ACCOUNT_LEVEL enums)
    int64_t SerialKey;
    int32_t iID;
    uint64_t FEKey;
    time_t creationTime;

    int level;
    int HP;
    int slot; // player slot, not nano slot
    int16_t mentor;
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
    int32_t iConditionBitFlag;
    int8_t iSpecialState;

    int x, y, z, angle;
    int lastX, lastY, lastZ, lastAngle;
    uint64_t instanceID;
    sItemBase Equip[AEQUIP_COUNT];
    sItemBase Inven[AINVEN_COUNT];
    sItemBase Bank[ABANK_COUNT];
    sItemTrade Trade[12];
    int32_t moneyInTrade;
    bool isTrading;
    bool isTradeConfirm;

    bool inCombat;
    bool passiveNanoOut;

    int pointDamage;
    int groupDamage;
    int defense;

    int64_t aQuestFlag[16];
    int tasks[ACTIVE_MISSION_COUNT];
    int RemainingNPCCount[ACTIVE_MISSION_COUNT][3];
    sItemBase QInven[AQINVEN_COUNT];
    int32_t CurrentMissionID;

    sTimeLimitItemDeleteInfo2CL toRemoveVehicle;

    int32_t iIDGroup;
    int groupCnt;
    int32_t groupIDs[4];
    int32_t iGroupConditionBitFlag;
};
