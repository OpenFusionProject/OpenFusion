#pragma once

#include "core/Core.hpp"

/* forward declaration(s) */
struct Entity;

enum EntityKind {
    INVALID,
    PLAYER,
    SIMPLE_NPC,
    COMBAT_NPC,
    MOB,
    EGG,
    BUS
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

    bool operator!=(const EntityRef& other) const {
        return !(*this == other);
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