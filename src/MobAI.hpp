#pragma once

#include "core/Core.hpp"
#include "NPCManager.hpp"

enum class MobState {
    INACTIVE,
    ROAMING,
    COMBAT,
    RETREAT,
    DEAD
};

struct Mob : public BaseNPC {
    // general
    MobState state;
    int maxHealth;
    int spawnX;
    int spawnY;
    int spawnZ;
    int level;

    std::unordered_map<int32_t,time_t> unbuffTimes;

    // dead
    time_t killedTime = 0;
    time_t regenTime;
    bool summoned = false;
    bool despawned = false; // for the sake of death animations

    // roaming
    int idleRange;
    const int sightRange;
    time_t nextMovement = 0;
    bool staticPath = false;
    int roamX, roamY, roamZ;

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;
    time_t lastDrainTime = 0;
    int skillStyle = -1; // -1 for nothing, 0-2 for corruption, -2 for eruption
    int hitX, hitY, hitZ; // for use in ability targeting

    // drop
    int dropType;

    // group
    int groupLeader = 0;
    int offsetX, offsetY;
    int groupMember[4] = {0, 0, 0, 0};

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int angle, uint64_t iID, int type, nlohmann::json d, int32_t id)
        : BaseNPC(x, y, z, angle, iID, type, id),
          maxHealth(d["m_iHP"]),
          sightRange(d["m_iSightRange"]) {
        state = MobState::ROAMING;

        data = d;

        regenTime = data["m_iRegenTime"];
        idleRange = (int)data["m_iIdleRange"];
        dropType = data["m_iDropType"];
        level = data["m_iNpcLevel"];

        roamX = spawnX = appearanceData.iX;
        roamY = spawnY = appearanceData.iY;
        roamZ = spawnZ = appearanceData.iZ;

        offsetX = 0;
        offsetY = 0;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;

        npcClass = NPC_MOB;
    }

    // constructor for /summon
    Mob(int x, int y, int z, uint64_t iID, int type, nlohmann::json d, int32_t id)
        : Mob(x, y, z, 0, iID, type, d, id) {
        summoned = true; // will be despawned and deallocated when killed
    }

    ~Mob() {}

    auto operator[](std::string s) {
        return data[s];
    }
};

namespace MobAI {
    extern bool simulateMobs;
    extern std::map<int32_t, Mob*> Mobs;

    void init();

    // TODO: make this internal later
    void incNextMovement(Mob *mob, time_t currTime=0);
    bool aggroCheck(Mob *mob, time_t currTime);
    void clearDebuff(Mob *mob);
    void followToCombat(Mob *mob);
    void groupRetreat(Mob *mob);
    void enterCombat(CNSocket *sock, Mob *mob);
}
