#pragma once

#include "core/Core.hpp"

#include "EntityRef.hpp"

#include <vector>
#include <functional>

/* forward declaration(s) */
class Buff;
template<class... Types>
using BuffCallback = std::function<void(EntityRef, Buff*, Types...)>;

#define CSB_FROM_ECSB(x) (1 << (x - 1))

enum class BuffClass {
    NONE = ETBT_NONE,
	NANO = ETBT_NANO,
	GROUP_NANO = ETBT_GROUPNANO,
	EGG = ETBT_SHINY,
	ENVIRONMENT = ETBT_LANDEFFECT,
	ITEM = ETBT_ITEM,
	CASH_ITEM = ETBT_CASHITEM
};

enum class BuffValueSelector {
    MAX_VALUE,
    MIN_VALUE,
    MAX_MAGNITUDE,
    MIN_MAGNITUDE,
    NET_TOTAL
};

struct BuffStack {
    int durationTicks;
    int value;
    EntityRef source;
    BuffClass buffStackClass;
};

class Buff {
private:
    EntityRef self;
    std::vector<BuffStack> stacks;

public:
    int id;
    /* called just after a stack is added or removed */
    BuffCallback<int, BuffStack*> onUpdate;
    /* called when the buff is combat-ticked */
    BuffCallback<time_t> onCombatTick;

    void tick(time_t);
    void combatTick(time_t);
    void clear();
    void clear(BuffClass buffClass);
    void addStack(BuffStack* stack);

    /*
    * Sometimes we need to determine if a buff
    * is covered by a certain class, ex: nano
    * vs. coco egg in the case of infection protection
    */
    bool hasClass(BuffClass buffClass);
    BuffClass maxClass();

    int getValue(BuffValueSelector selector);

    /* 
     * In general, a Buff object won't exist
     * unless it has stacks. However, when
     * popping stacks during iteration (onExpire),
     * stacks will be empty for a brief moment
     * when the last stack is popped.
     */
    bool isStale();

    void updateCallbacks(BuffCallback<int, BuffStack*> fOnUpdate, BuffCallback<time_t> fonTick);

    Buff(int iid, EntityRef pSelf, BuffCallback<int, BuffStack*> fOnUpdate, BuffCallback<time_t> fOnCombatTick, BuffStack* firstStack)
        : self(pSelf), id(iid), onUpdate(fOnUpdate), onCombatTick(fOnCombatTick) {
        addStack(firstStack);
    }
};

namespace Buffs {
    void timeBuffUpdate(EntityRef self, Buff* buff, int status, BuffStack* stack);
    void timeBuffTimeout(EntityRef self);
}
