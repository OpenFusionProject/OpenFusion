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
    MobState state;
    const int maxHealth;
    time_t killedTime = 0;
    const int regenTime;
    bool despawned = false; // for the sake of death animations

    int spawnX;
    int spawnY;
    int spawnZ;

    const int idleRange;
    time_t nextMovement = 0;

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int type, int hp, int angle, nlohmann::json d)
        : BaseNPC(x, y, z, type), maxHealth(hp), regenTime(d["m_iRegenTime"]), idleRange(d["m_iIdleRange"]), data(d) {
        state = MobState::ROAMING;

        spawnX = appearanceData.iX;
        spawnY = appearanceData.iY;
        spawnZ = appearanceData.iZ;

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
    void step(time_t);

    void deadStep(Mob*, time_t);
    void roamingStep(Mob*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);

    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock);
}
