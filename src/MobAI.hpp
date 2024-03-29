#pragma once

#include "core/Core.hpp"
#include "JSON.hpp"

#include "Entities.hpp"

#include <unordered_map>
#include <string>

namespace MobAI {
    void deadStep(CombatNPC* self, time_t currTime);
    void combatStep(CombatNPC* self, time_t currTime);
    void roamingStep(CombatNPC* self, time_t currTime);
    void retreatStep(CombatNPC* self, time_t currTime);

    void onRoamStart(CombatNPC* self, EntityRef src);
    void onCombatStart(CombatNPC* self, EntityRef src);
    void onRetreat(CombatNPC* self, EntityRef src);
    void onDeath(CombatNPC* self, EntityRef src);
}

struct Mob : public CombatNPC {

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

    // temporary; until we're sure what's what
    nlohmann::json data = {};

    Mob(int spawnX, int spawnY, int spawnZ, int angle, uint64_t iID, int t, nlohmann::json d, int32_t id)
        : CombatNPC(spawnX, spawnY, spawnZ, angle, iID, t, id, d["m_iHP"]),
          sightRange(d["m_iSightRange"]) {
        state = AIState::ROAMING;

        data = d;

        speed = data["m_iRunSpeed"];
        regenTime = data["m_iRegenTime"];
        idleRange = (int)data["m_iIdleRange"];
        level = data["m_iNpcLevel"];

        roamX = spawnX;
        roamY = spawnY;
        roamZ = spawnZ;

        offsetX = 0;
        offsetY = 0;

        // NOTE: there appear to be discrepancies in the dump
        hp = maxHealth;

        kind = EntityKind::MOB;

        // AI
        stateHandlers[AIState::DEAD] = MobAI::deadStep;
        stateHandlers[AIState::COMBAT] = MobAI::combatStep;
        stateHandlers[AIState::ROAMING] = MobAI::roamingStep;
        stateHandlers[AIState::RETREAT] = MobAI::retreatStep;

        transitionHandlers[AIState::DEAD] = MobAI::onDeath;
        transitionHandlers[AIState::COMBAT] = MobAI::onCombatStart;
        transitionHandlers[AIState::ROAMING] = MobAI::onRoamStart;
        transitionHandlers[AIState::RETREAT] = MobAI::onRetreat;
    }

    // constructor for /summon
    Mob(int x, int y, int z, uint64_t iID, int t, nlohmann::json d, int32_t id)
        : Mob(x, y, z, 0, iID, t, d, id) {
        summoned = true; // will be despawned and deallocated when killed
    }

    ~Mob() {}

    virtual int takeDamage(EntityRef src, int amt) override;
    virtual void step(time_t currTime) override;

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
}
