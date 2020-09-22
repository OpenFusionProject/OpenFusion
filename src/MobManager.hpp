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
    int maxHealth;
    int spawnX;
    int spawnY;
    int spawnZ;

    // dead
    time_t killedTime = 0;
    time_t regenTime;
    bool summoned = false;
    bool despawned = false; // for the sake of death animations

    // roaming
    int idleRange;
    time_t nextMovement = 0;

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int type, int hp, int angle, nlohmann::json d, int32_t id)
        : BaseNPC(x, y, z, type, id), maxHealth(hp) {
        state = MobState::ROAMING;

        data = d;

        regenTime = data["m_iRegenTime"];
        idleRange = data["m_iIdleRange"];

        // XXX: temporarily force respawns for Fusions until we implement instancing
        if (regenTime >= 300000000)
            regenTime = 1500;

        spawnX = appearanceData.iX;
        spawnY = appearanceData.iY;
        spawnZ = appearanceData.iZ;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;

        npcClass = NPC_MOB;
    }

    // constructor for /summon
    Mob(int x, int y, int z, int type, nlohmann::json d, int32_t id)
        : Mob(x, y, z, type, 0, 0, d, id) {
        summoned = true; // will be despawned and deallocated when killed
        appearanceData.iHP = maxHealth = d["m_iHP"];
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
    void playerTick(CNServer*, time_t);

    void deadStep(Mob*, time_t);
    void combatStep(Mob*, time_t);
    void retreatStep(Mob*, time_t);
    void roamingStep(Mob*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);
    void dealGooDamage(CNSocket *sock, int amount);

    void npcAttackPc(Mob *mob);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock);
    std::pair<int,int> lerp(int, int, int, int, int);
    std::pair<int,int> getDamage(int, int, bool, int);

    void pcAttackChars(CNSocket *sock, CNPacketData *data);
}
