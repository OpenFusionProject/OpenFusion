#pragma once

#include "core/Core.hpp"
#include "Chunking.hpp"

#include <stdint.h>
#include <set>

enum EntityKind {
    INVALID,
    PLAYER,
    SIMPLE_NPC,
    COMBAT_NPC,
    MOB,
    EGG,
    BUS
};

enum class AIState {
    INACTIVE,
    ROAMING,
    COMBAT,
    RETREAT,
    DEAD
};

class Chunk;
struct Group;

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

struct EntityRef {
    EntityKind kind;
    union {
        CNSocket *sock;
        int32_t id;
    };

    EntityRef(CNSocket *s);
    EntityRef(int32_t i);

    bool isValid() const;
    Entity *getEntity() const;

    bool operator==(const EntityRef& other) const {
        if (kind != other.kind)
            return false;

        if (kind == EntityKind::PLAYER)
            return sock == other.sock;

        return id == other.id;
    }

    // arbitrary ordering
    bool operator<(const EntityRef& other) const {
        if (kind == other.kind) {
            if (kind == EntityKind::PLAYER)
                return sock < other.sock;
            else
                return id < other.id;
        }

        return kind < other.kind;
    }
};

/*
 * Interfaces
 */
class ICombatant {
public:
    ICombatant() {}
    virtual ~ICombatant() {}

    virtual int takeDamage(EntityRef, int) = 0;
    virtual void heal(EntityRef, int) = 0;
    virtual bool isAlive() = 0;
    virtual int getCurrentHP() = 0;
    virtual int32_t getID() = 0;
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
    int cbf;
    bool loopingPath = false;

    BaseNPC(int _A, uint64_t iID, int t, int _id) {
        type = t;
        hp = 400;
        angle = _A;
        cbf = 0;
        id = _id;
        instanceID = iID;
    };

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;

    sNPCAppearanceData getAppearanceData();
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

    CombatNPC(int x, int y, int z, int angle, uint64_t iID, int t, int id, int maxHP)
        : BaseNPC(angle, iID, t, id), maxHealth(maxHP) {
        spawnX = x;
        spawnY = y;
        spawnZ = z;

        stateHandlers[AIState::INACTIVE] = {};
        transitionHandlers[AIState::INACTIVE] = {};
    }

    virtual bool isExtant() override { return hp > 0; }

    virtual int takeDamage(EntityRef src, int amt) override;
    virtual void heal(EntityRef src, int amt) override;
    virtual bool isAlive() override;
    virtual int getCurrentHP() override;
    virtual int32_t getID() override;
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
