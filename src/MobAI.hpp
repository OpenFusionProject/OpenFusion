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

namespace MobAI {
    // needs to be declared before Mob's constructor
    void step(CombatNPC*, time_t);
};

struct Mob : public CombatNPC {
    // general
    MobState state = MobState::INACTIVE;

    std::unordered_map<int32_t,time_t> unbuffTimes = {};

    // dead
    time_t killedTime = 0;
    time_t regenTime = 0;
    bool summoned = false;
    bool despawned = false; // for the sake of death animations

    // roaming
    int idleRange = 0;
    const int sightRange = 0;
    time_t nextMovement = 0;
    bool staticPath = false;
    int roamX = 0, roamY = 0, roamZ = 0;

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;
    time_t lastDrainTime = 0;
    int skillStyle = -1; // -1 for nothing, 0-2 for corruption, -2 for eruption
    int hitX = 0, hitY = 0, hitZ = 0; // for use in ability targeting

    // group
    int groupLeader = 0;
    int offsetX = 0, offsetY = 0;
    int groupMember[4] = {};

    // for optimizing away AI in empty chunks
    int playersInView = 0;

    // temporary; until we're sure what's what
    nlohmann::json data = {};

    Mob(int x, int y, int z, int angle, uint64_t iID, int t, nlohmann::json d, int32_t id)
        : CombatNPC(x, y, z, angle, iID, t, id, d["m_iHP"]),
          sightRange(d["m_iSightRange"]) {
        state = MobState::ROAMING;

        data = d;

        speed = data["m_iRunSpeed"];
        regenTime = data["m_iRegenTime"];
        idleRange = (int)data["m_iIdleRange"];
        level = data["m_iNpcLevel"];

        roamX = spawnX = x;
        roamY = spawnY = y;
        roamZ = spawnZ = z;

        offsetX = 0;
        offsetY = 0;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;

        type = EntityType::MOB;
        _stepAI = MobAI::step;
    }

    // constructor for /summon
    Mob(int x, int y, int z, uint64_t iID, int t, nlohmann::json d, int32_t id)
        : Mob(x, y, z, 0, iID, t, d, id) {
        summoned = true; // will be despawned and deallocated when killed
    }

    ~Mob() {}

    auto operator[](std::string s) {
        return data[s];
    }
};

namespace MobAI {
    extern bool simulateMobs;

    // TODO: make this internal later
    void incNextMovement(Mob *mob, time_t currTime=0);
    bool aggroCheck(Mob *mob, time_t currTime);
    void clearDebuff(Mob *mob);
    void followToCombat(Mob *mob);
    void groupRetreat(Mob *mob);
    void enterCombat(CNSocket *sock, Mob *mob);
}
