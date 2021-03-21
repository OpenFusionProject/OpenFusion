#pragma once

#include <string>
#include <cstring>

#include "core/Core.hpp"
#include "Chunking.hpp"
#include "Entities.hpp"

#define ACTIVE_MISSION_COUNT 6

#define PC_MAXHEALTH(level) (925 + 75 * (level))

struct Player : public Entity {
    int accountId = 0;
    int accountLevel = 0; // permission level (see CN_ACCOUNT_LEVEL enums)
    int64_t SerialKey = 0;
    int32_t iID = 0;
    uint64_t FEKey = 0;

    int level = 0;
    int HP = 0;
    int slot = 0; // player slot, not nano slot
    int16_t mentor = 0;
    int32_t money = 0;
    int32_t fusionmatter = 0;
    int32_t batteryW = 0;
    int32_t batteryN = 0;
    sPCStyle PCStyle = {};
    sPCStyle2 PCStyle2 = {};
    sNano Nanos[NANO_COUNT] = {}; // acquired nanos
    int equippedNanos[3] = {};
    int activeNano = 0; // active nano (index into Nanos)
    int8_t iPCState = 0;
    int32_t iWarpLocationFlag = 0;
    int64_t aSkywayLocationFlag[2] = {};
    int32_t iConditionBitFlag = 0;
    int32_t iSelfConditionBitFlag = 0;
    int8_t iSpecialState = 0;

    int angle = 0;
    int lastX = 0, lastY = 0, lastZ = 0, lastAngle = 0;
    int recallX = 0, recallY = 0, recallZ = 0, recallInstance = 0; // also Lair entrances
    sItemBase Equip[AEQUIP_COUNT] = {};
    sItemBase Inven[AINVEN_COUNT] = {};
    sItemBase Bank[ABANK_COUNT] = {};
    sItemTrade Trade[12] = {};
    int32_t moneyInTrade = 0;
    bool isTrading = false;
    bool isTradeConfirm = false;

    bool inCombat = false;
    bool onMonkey = false;
    int nanoDrainRate = 0;
    int healCooldown = 0;

    int pointDamage = 0;
    int groupDamage = 0;
    int fireRate = 0;
    int defense = 0;

    int64_t aQuestFlag[16] = {};
    int tasks[ACTIVE_MISSION_COUNT] = {};
    int RemainingNPCCount[ACTIVE_MISSION_COUNT][3] = {};
    sItemBase QInven[AQINVEN_COUNT] = {};
    int32_t CurrentMissionID = 0;

    sTimeLimitItemDeleteInfo2CL toRemoveVehicle = {};

    int32_t iIDGroup = 0;
    int groupCnt = 0;
    int32_t groupIDs[4] = {};
    int32_t iGroupConditionBitFlag = 0;

    bool notify = false;
    bool hidden = false;
    bool unwarpable = false;

    bool buddiesSynced = false;
    int64_t buddyIDs[50] = {};
    bool isBuddyBlocked[50] = {};

    uint64_t iFirstUseFlag[2] = {};
    time_t lastHeartbeat = 0;

    int suspicionRating = 0;
    time_t lastShot = 0;
    std::vector<sItemBase> buyback = {};

    Player() { type = EntityType::PLAYER; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};
