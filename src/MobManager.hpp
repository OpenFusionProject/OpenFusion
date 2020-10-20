#pragma once

#include "CNProtocol.hpp"
#include "CNShared.hpp"
#include "CNShardServer.hpp"
#include "NPC.hpp"

#include "contrib/JSON.hpp"

#include <map>
#include <queue>

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

    // combat
    CNSocket *target = nullptr;
    time_t nextAttack = 0;
    int roamX, roamY, roamZ;

    // drop
    int dropType;

    // temporary; until we're sure what's what
    nlohmann::json data;

    Mob(int x, int y, int z, int angle, uint64_t iID, int type, int hp, nlohmann::json d, int32_t id)
        : BaseNPC(x, y, z, angle, iID, type, id),
          maxHealth(hp),
          sightRange(d["m_iSightRange"]) {
        state = MobState::ROAMING;

        data = d;

        regenTime = data["m_iRegenTime"];
        idleRange = (int)data["m_iIdleRange"] * 2; // TODO: tuning?
        dropType = data["m_iDropType"];
        level = data["m_iNpcLevel"];

        roamX = spawnX = appearanceData.iX;
        roamY = spawnY = appearanceData.iY;
        roamZ = spawnZ = appearanceData.iZ;

        appearanceData.iConditionBitFlag = 0;

        // NOTE: there appear to be discrepancies in the dump
        appearanceData.iHP = maxHealth;

        npcClass = NPC_MOB;
    }

    // constructor for /summon
    Mob(int x, int y, int z, uint64_t iID, int type, nlohmann::json d, int32_t id)
        : Mob(x, y, z, 0, iID, type, 0, d, id) {
        summoned = true; // will be despawned and deallocated when killed
        appearanceData.iHP = maxHealth = d["m_iHP"];
    }

    ~Mob() {}

    auto operator[](std::string s) {
        return data[s];
    }
};

struct MobDropChance {
    int dropChance;
    std::vector<int> cratesRatio;
};

struct MobDrop {
    std::vector<int> crateIDs;
    int dropChanceType;
    int taros;
    int fm;
    int boosts;
};

namespace MobManager {
    extern std::map<int32_t, Mob*> Mobs;
    extern std::queue<int32_t> RemovalQueue;
    extern std::map<int32_t, MobDropChance> MobDropChances;
    extern std::map<int32_t, MobDrop> MobDrops;
    extern bool simulateMobs;

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

    void npcAttackPc(Mob *mob, time_t currTime);
    int hitMob(CNSocket *sock, Mob *mob, int damage);
    void killMob(CNSocket *sock, Mob *mob);
    void giveReward(CNSocket *sock, Mob *mob);
    void getReward(sItemBase *reward, MobDrop *drop, MobDropChance *chance);
    void giveEventReward(CNSocket* sock, Player* player);

    std::pair<int,int> lerp(int, int, int, int, int);
    std::pair<int,int> getDamage(int, int, bool, bool, int, int, int);

    void pcAttackChars(CNSocket *sock, CNPacketData *data);
    void resendMobHP(Mob *mob);
    void incNextMovement(Mob *mob, time_t currTime=0);
    bool aggroCheck(Mob *mob, time_t currTime);
}
