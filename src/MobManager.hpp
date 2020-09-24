#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
#include "NPC.hpp"

#include "contrib/JSON.hpp"

#include <map>

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
    const int maxHealth;
    int spawnX;
    int spawnY;
    int spawnZ;

    // dead
    time_t killedTime = 0;
    int regenTime;
    bool despawned = false; // for the sake of death animations

    // roaming
    int idleRange;
    time_t nextMovement = 0;

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int type, int hp, int angle, nlohmann::json d)
        : BaseNPC(x, y, z, type), maxHealth(hp) {
        state = MobState::ROAMING;

        data = d;

        regenTime = data["m_iRegenTime"];
        idleRange = data["m_iIdleRange"];

        spawnX = appearanceData.iX;
        spawnY = appearanceData.iY;
        spawnZ = appearanceData.iZ;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;
    }
    ~Mob() {}

    auto operator[](std::string s) {
        return data[s];
    }
};

namespace MobManager {
    extern std::map<int32_t, Mob*> Mobs;

    void init();
    void step(CNServer*, time_t);

    void deadStep(Mob*, time_t);
    void combatStep(Mob*, time_t);
    void retreatStep(Mob*, time_t);
    void roamingStep(Mob*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);

    void npcAttackPc(Mob *mob);
    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock);
}
