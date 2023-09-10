#pragma once

#include "core/Core.hpp"

#include "EntityRef.hpp"
#include "Buffs.hpp"
#include "Chunking.hpp"
#include "Groups.hpp"

#include <set>
#include <map>
#include <functional>

enum class AIState {
    INACTIVE,
    ROAMING,
    COMBAT,
    RETREAT,
    DEAD
};

struct Entity {
    EntityKind kind = EntityKind::INVALID;
    int x = 0, y = 0, z = 0;
    uint64_t instanceID = 0;
    ChunkPos chunkPos = {};
    std::set<Chunk*> viewableChunks = {};

    // destructor must be virtual, apparently
    virtual ~Entity() {}

    virtual bool isExtant() { return true; }

    // stubs
    virtual void enterIntoViewOf(CNSocket *sock) = 0;
    virtual void disappearFromViewOf(CNSocket *sock) = 0;
};

/*
 * Interfaces
 */
class ICombatant {
public:
    ICombatant() {}
    virtual ~ICombatant() {}

    virtual bool addBuff(int, BuffCallback<int, BuffStack*>, BuffCallback<time_t>, BuffStack*) = 0;
    virtual Buff* getBuff(int) = 0;
    virtual void removeBuff(int) = 0;
    virtual void removeBuff(int, BuffClass) = 0;
    virtual void clearBuffs(bool) = 0;
    virtual bool hasBuff(int) = 0;
    virtual int getCompositeCondition() = 0;
    virtual int takeDamage(EntityRef, int) = 0;
    virtual int heal(EntityRef, int) = 0;
    virtual bool isAlive() = 0;
    virtual int getCurrentHP() = 0;
    virtual int getMaxHP() = 0;
    virtual int getLevel() = 0;
    virtual std::vector<EntityRef> getGroupMembers() = 0;
    virtual int32_t getCharType() = 0;
    virtual int32_t getID() = 0;
    virtual EntityRef getRef() = 0;
    virtual void step(time_t currTime) = 0;
};

/*
 * Subclasses
 */
class BaseNPC : public Entity {
public:
    int id;
    int type;
    int hp;
    int angle;
    bool loopingPath = false;

    BaseNPC(int _A, uint64_t iID, int t, int _id) {
        kind = EntityKind::SIMPLE_NPC;
        type = t;
        hp = 400;
        angle = _A;
        id = _id;
        instanceID = iID;
    };

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;

    virtual sNPCAppearanceData getAppearanceData();
};

struct CombatNPC : public BaseNPC, public ICombatant {
    int maxHealth = 0;
    int spawnX = 0;
    int spawnY = 0;
    int spawnZ = 0;
    int level = 0;
    int speed = 300;
    AIState state = AIState::INACTIVE;
    Group* group = nullptr;
    int playersInView = 0; // for optimizing away AI in empty chunks

    std::map<AIState, void (*)(CombatNPC*, time_t)> stateHandlers;
    std::map<AIState, void (*)(CombatNPC*, EntityRef)> transitionHandlers;

    std::unordered_map<int, Buff*> buffs = {};

    CombatNPC(int spawnX, int spawnY, int spawnZ, int angle, uint64_t iID, int t, int id, int maxHP)
        : BaseNPC(angle, iID, t, id), maxHealth(maxHP) {
        this->spawnX = spawnX;
        this->spawnY = spawnY;
        this->spawnZ = spawnZ;

        kind = EntityKind::COMBAT_NPC;

        stateHandlers[AIState::INACTIVE] = {};
        transitionHandlers[AIState::INACTIVE] = {};
    }

    virtual sNPCAppearanceData getAppearanceData() override;

    virtual bool isExtant() override { return hp > 0; }

    virtual bool addBuff(int buffId, BuffCallback<int, BuffStack*> onUpdate, BuffCallback<time_t> onTick, BuffStack* stack) override;
    virtual Buff* getBuff(int buffId) override;
    virtual void removeBuff(int buffId) override;
    virtual void removeBuff(int buffId, BuffClass buffClass) override;
    virtual void clearBuffs(bool force) override;
    virtual bool hasBuff(int buffId) override;
    virtual int getCompositeCondition() override;
    virtual int takeDamage(EntityRef src, int amt) override;
    virtual int heal(EntityRef src, int amt) override;
    virtual bool isAlive() override;
    virtual int getCurrentHP() override;
    virtual int getMaxHP() override;
    virtual int getLevel() override;
    virtual std::vector<EntityRef> getGroupMembers() override;
    virtual int32_t getCharType() override;
    virtual int32_t getID() override;
    virtual EntityRef getRef() override;
    virtual void step(time_t currTime) override;

    virtual void transition(AIState newState, EntityRef src);
};

// Mob is in MobAI.hpp, Player is in Player.hpp

struct Egg : public BaseNPC {
    bool summoned = false;
    bool dead = false;
    time_t deadUntil;

    Egg(uint64_t iID, int t, int32_t id, bool summon)
        : BaseNPC(0, iID, t, id) {
        summoned = summon;
        kind = EntityKind::EGG;
    }

    virtual bool isExtant() override { return !dead; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

struct Bus : public BaseNPC {
    Bus(int angle, uint64_t iID, int t, int id) :
        BaseNPC(angle, iID, t, id) {
        kind = EntityKind::BUS;
        loopingPath = true;
    }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};
