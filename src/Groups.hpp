#pragma once

#include "EntityRef.hpp"

#include <vector>
#include <assert.h>

struct Group {
    std::vector<EntityRef> members;

    std::vector<EntityRef> filter(EntityKind kind) {
        std::vector<EntityRef> filtered;
        std::copy_if(members.begin(), members.end(), std::back_inserter(filtered), [kind](EntityRef e) {
            return e.kind == kind;
            });
        return filtered;
    }
    EntityRef getLeader() {
        assert(members.size() > 0);
        return members[0];
    }

    Group(EntityRef leader);
};

namespace Groups {
    void init();

    void sendToGroup(Group* group, void* buf, uint32_t type, size_t size);
    void sendToGroup(Group* group, EntityRef excluded, void* buf, uint32_t type, size_t size);
    void groupTickInfo(CNSocket* sock);

    void groupKick(Group* group, EntityRef ref);
    void addToGroup(Group* group, EntityRef member);
    bool removeFromGroup(Group* group, EntityRef member); // true iff group deleted
    void disbandGroup(Group* group);
}
