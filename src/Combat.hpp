#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
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
    void playerTick(CNServer*, time_t);

    void pcAttackNpcs(CNSocket *sock, CNPacketData *data);
    void combatBegin(CNSocket *sock, CNPacketData *data);
    void combatEnd(CNSocket *sock, CNPacketData *data);
    void dotDamageOnOff(CNSocket *sock, CNPacketData *data);
    void dealGooDamage(CNSocket *sock, int amount);

    void npcAttackPc(Mob *mob, time_t currTime);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);

    std::pair<int,int> lerp(int, int, int, int, int);
    std::pair<int,int> getDamage(int, int, bool, bool, int, int, int);

    void pcAttackChars(CNSocket *sock, CNPacketData *data);
    void grenadeFire(CNSocket* sock, CNPacketData* data);
    void rocketFire(CNSocket* sock, CNPacketData* data);
    void projectileHit(CNSocket* sock, CNPacketData* data);
    /// returns bullet id
    int8_t addBullet(Player* plr, bool isGrenade);
}
