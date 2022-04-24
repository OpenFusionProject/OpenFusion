#pragma once

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"
#include "Entities.hpp"

#include <map>
#include <list>

struct Player;
enum EntityKind;

struct Group {
    std::vector<EntityRef> members;
    int32_t conditionBitFlag;

    std::vector<EntityRef> filter(EntityKind kind) {
        std::vector<EntityRef> filtered;
        std::copy_if(members.begin(), members.end(), std::back_inserter(filtered), [kind](EntityRef e) {
            return e.kind == kind;
            });
        return filtered;
    }
};

namespace Groups {
    void init();

    void sendToGroup(Group* group, void* buf, uint32_t type, size_t size);
    void groupTickInfo(Player* plr);
    void groupKick(Player* plr);

    void addToGroup(EntityRef member, Group* group);
    void removeFromGroup(EntityRef member, Group* group);
    void disbandGroup(Group* group);
}
