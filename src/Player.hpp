#pragma once

#include "core/Core.hpp"

#include "Entities.hpp"
#include "Groups.hpp"

#include <vector>

/* forward declaration(s) */
class Buff;
struct BuffStack;

#define ACTIVE_MISSION_COUNT 6

#define PC_MAXHEALTH(level) (925 + 75 * (level))

struct Player : public Entity, public ICombatant {
    int accountId = 0;
    int accountLevel = 0; // permission level (see CN_ACCOUNT_LEVEL enums)
    int32_t iID = 0;

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
    int8_t iSpecialState = 0;
    std::unordered_map<int, Buff*> buffs = {};

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

    Group* group = nullptr;

    bool notify = false;
    bool hidden = false;
    bool unwarpable = false;
    bool initialLoadDone = false;

    int64_t buddyIDs[50] = {};
    bool isBuddyBlocked[50] = {};

    uint64_t iFirstUseFlag[2] = {};
    time_t lastHeartbeat = 0;

    int suspicionRating = 0;
    time_t lastShot = 0;
    std::vector<sItemBase> buyback = {};

    Player() { kind = EntityKind::PLAYER; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;

    virtual bool addBuff(int buffId, BuffCallback<int, BuffStack*> onUpdate, BuffCallback<time_t> onTick, BuffStack* stack) override;
    virtual Buff* getBuff(int buffId) override;
    virtual void removeBuff(int buffId) override;
    virtual void removeBuff(int buffId, BuffClass buffClass) override;
    virtual void clearBuffs(bool force) override;
    virtual bool hasBuff(int buffId) override;
    virtual int getCompositeCondition() override;
    virtual int takeDamage(EntityRef src, int amt) override;
    virtual int heal(EntityRef src, int amt) override;
    virtual bool isAlive() override;
    virtual int getCurrentHP() override;
    virtual int getMaxHP() override;
    virtual int getLevel() override;
    virtual std::vector<EntityRef> getGroupMembers() override;
    virtual int32_t getCharType() override;
    virtual int32_t getID() override;
    virtual EntityRef getRef() override;

    virtual void step(time_t currTime) override;

    sNano* getActiveNano();
    sPCAppearanceData getAppearanceData();
};
