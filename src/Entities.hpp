#pragma once

#include "core/Core.hpp"

#include <stdint.h>
#include <set>

enum class EntityType {
    PLAYER,
    SIMPLE_NPC,
    COMBAT_NPC,
    EGG,
    BUS
};

class Chunk;

struct Entity {
    EntityType type;
    int x, y, z;
    uint64_t instanceID;
    ChunkPos chunkPos;
    std::set<Chunk*> viewableChunks;

    // destructor must be virtual, apparently
    virtual ~Entity() {}

    virtual bool isAlive() { return true; }

    // stubs
    virtual void enterIntoViewOf(CNSocket *sock) {}
    virtual void disappearFromViewOf(CNSocket *sock) {}
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

class BaseNPC : public Entity {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;

    int playersInView;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id) { // XXX
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = angle;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        npcClass = NPCClass::NPC_BASE;

        instanceID = iID;

        chunkPos = std::make_tuple(0, 0, 0);
        playersInView = 0;
    };
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id, NPCClass classType) : BaseNPC(x, y, z, angle, iID, type, id) {
        npcClass = classType;
    }

    // XXX: move to CombatNPC, probably
    virtual bool isAlive() override { return appearanceData.iHP > 0; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

// TODO: decouple from BaseNPC
struct Egg : public BaseNPC {
    bool summoned;
    bool dead = false;
    time_t deadUntil;

    Egg(int x, int y, int z, uint64_t iID, int type, int32_t id, bool summon)
        : BaseNPC(x, y, z, 0, iID, type, id) {
        summoned = summon;
        npcClass = NPCClass::NPC_EGG;
    }

    virtual bool isAlive() override { return !dead; }

    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

// TODO: decouple from BaseNPC
struct Bus : public BaseNPC {
    virtual void enterIntoViewOf(CNSocket *sock) override;
    virtual void disappearFromViewOf(CNSocket *sock) override;
};

#if 0
struct NPC : public Entity {
    void (*_stepAI)();

    virtual void stepAI() {}
};

struct CombatNPC : public Entity {
};

struct Mob : public CombatNPC {
};
#endif
