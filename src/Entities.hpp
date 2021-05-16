#pragma once

#include "core/Core.hpp"

#include <stdint.h>
#include <set>

enum class EntityType : uint8_t {
    INVALID,
    PLAYER,
    SIMPLE_NPC,
    COMBAT_NPC,
    MOB,
    EGG,
    BUS
};

class Chunk;

struct Entity {
    EntityType type = EntityType::INVALID;
    int x = 0, y = 0, z = 0;
    uint64_t instanceID = 0;
    ChunkPos chunkPos = {};
    std::set<Chunk*> viewableChunks = {};

    // destructor must be virtual, apparently
    virtual ~Entity() {}

    virtual bool isAlive() { return true; }

    // stubs
    virtual void enterIntoViewOf(CNSocket *sock) = 0;
    virtual void disappearFromViewOf(CNSocket *sock) = 0;
};

struct EntityRef {
    EntityType type;
    union {
        CNSocket *sock;
        int32_t id;
    };

    EntityRef(CNSocket *s);
    EntityRef(int32_t i);

    bool isValid() const;
    Entity *getEntity() const;

    bool operator==(const EntityRef& other) const {
        if (type != other.type)
            return false;

        if (type == EntityType::PLAYER)
            return sock == other.sock;

        return id == other.id;
    }

    // arbitrary ordering
    bool operator<(const EntityRef& other) const {
        if (type == other.type) {
            if (type == EntityType::PLAYER)
                return sock < other.sock;
            else
                return id < other.id;
        }

        return type < other.type;
    }
};

/*
 * Subclasses
 */
class BaseNPC : public Entity {
public:
    sNPCAppearanceData appearanceData = {};
    bool loopingPath = false;

    BaseNPC(int _X, int _Y, int _Z, int angle, uint64_t iID, int t, int id) { // XXX
        x = _X;
        y = _Y;
        z = _Z;
        appearanceData.iNPCType = t;
        appearanceData.iHP = 400;
        appearanceData.iAngle = angle;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        instanceID = iID;
    };

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

struct CombatNPC : public BaseNPC {
    int maxHealth = 0;
    int spawnX = 0;
    int spawnY = 0;
    int spawnZ = 0;
    int level = 0;
    int speed = 300;

    void (*_stepAI)(CombatNPC*, time_t) = nullptr;

    // XXX
    CombatNPC(int x, int y, int z, int angle, uint64_t iID, int t, int id, int maxHP) :
        BaseNPC(x, y, z, angle, iID, t, id),
        maxHealth(maxHP) {}

    virtual void stepAI(time_t currTime) {
        if (_stepAI != nullptr)
            _stepAI(this, currTime);
    }

    virtual bool isAlive() override { return appearanceData.iHP > 0; }
};

// Mob is in MobAI.hpp, Player is in Player.hpp

// TODO: decouple from BaseNPC
struct Egg : public BaseNPC {
    bool summoned = false;
    bool dead = false;
    time_t deadUntil;

    Egg(int x, int y, int z, uint64_t iID, int t, int32_t id, bool summon)
        : BaseNPC(x, y, z, 0, iID, t, id) {
        summoned = summon;
        type = EntityType::EGG;
    }

    virtual bool isAlive() override { return !dead; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

// TODO: decouple from BaseNPC
struct Bus : public BaseNPC {
    Bus(int x, int y, int z, int angle, uint64_t iID, int t, int id) :
        BaseNPC(x, y, z, angle, iID, t, id) {
        type = EntityType::BUS;
        loopingPath = true;
    }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};
