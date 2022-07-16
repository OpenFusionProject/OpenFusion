#pragma once

#include "Player.hpp"
#include "MobAI.hpp"

#include <map>
#include <vector>

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
    void genQItemRolls(std::vector<Player*> players, std::map<int, int>& rolls);
}
