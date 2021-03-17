#pragma once

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"
#include "NPC.hpp"
#include "MobAI.hpp"

#include "JSON.hpp"

#include <map>
#include <unordered_map>
#include <queue>

struct Bullet {
    int pointDamage;
    int groupDamage;
    bool weaponBoost;
    int bulletType;
};

namespace Combat {
    extern std::map<int32_t, std::map<int8_t, Bullet>> Bullets;

    void init();

    void npcAttackPc(Mob *mob, time_t currTime);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);
}
