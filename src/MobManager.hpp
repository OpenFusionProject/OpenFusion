#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
#include "NPC.hpp"

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

    Mob(int x, int y, int z, int type, int hp, int angle, int rt)
        : BaseNPC(x, y, z, type), maxHealth(hp), regenTime(rt) {
        state = MobState::ROAMING;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;
    }
};

namespace MobManager {
    extern std::map<int32_t, Mob*> Mobs;

    void init();
    void step(time_t);

    void deadStep(Mob*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);

    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock);
}
