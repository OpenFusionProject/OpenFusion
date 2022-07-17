#include "Buffs.hpp"

#include "PlayerManager.hpp"
#include "NPCManager.hpp"

using namespace Buffs;

void Buff::tick(time_t currTime) {
    auto it = stacks.begin();
    while(it != stacks.end()) {
        BuffStack& stack = *it;
        //if(onTick) onTick(self, this, currTime);

        if(stack.durationTicks == 0) {
            BuffStack deadStack = stack;
            it = stacks.erase(it);
            if(onUpdate) onUpdate(self, this, ETBU_DEL, &deadStack);
        } else {
            if(stack.durationTicks > 0) stack.durationTicks--;
            it++;
        }
    }
}

void Buff::combatTick(time_t currTime) {
    if(onCombatTick) onCombatTick(self, this, currTime);
}

void Buff::clear() {
    while(!stacks.empty()) {
        BuffStack stack = stacks.back();
        stacks.pop_back();
        if(onUpdate) onUpdate(self, this, ETBU_DEL, &stack);
    }
}

void Buff::clear(BuffClass buffClass) {
    auto it = stacks.begin();
    while(it != stacks.end()) {
        BuffStack& stack = *it;
        if(stack.buffStackClass == buffClass) {
            BuffStack deadStack = stack;
            it = stacks.erase(it);
            if(onUpdate) onUpdate(self, this, ETBU_DEL, &deadStack);
        } else it++;
    }
}

void Buff::addStack(BuffStack* stack) {
    stacks.push_back(*stack);
    if(onUpdate) onUpdate(self, this, ETBU_ADD, &stacks.back());
}

bool Buff::hasClass(BuffClass buffClass) {
    for(BuffStack& stack : stacks) {
        if(stack.buffStackClass == buffClass)
            return true;
    }
    return false;
}

BuffClass Buff::maxClass() {
    BuffClass buffClass = BuffClass::NONE;
    for(BuffStack& stack : stacks) {
        if(stack.buffStackClass > buffClass)
            buffClass = stack.buffStackClass;
    }
    return buffClass;
}

int Buff::getValue(BuffValueSelector selector) {
    if(isStale()) return 0;

    int value = selector == BuffValueSelector::NET_TOTAL ? 0 : stacks.front().value;
    for(BuffStack& stack : stacks) {
        switch(selector)
        {
            case BuffValueSelector::NET_TOTAL:
                value += stack.value;
                break;
            case BuffValueSelector::MIN_VALUE:
                if(stack.value < value) value = stack.value;
                break;
            case BuffValueSelector::MAX_VALUE:
                if(stack.value > value) value = stack.value;
                break;
            case BuffValueSelector::MIN_MAGNITUDE:
                if(abs(stack.value) < abs(value)) value = stack.value;
                break;
            case BuffValueSelector::MAX_MAGNITUDE:
            default:
                if(abs(stack.value) > abs(value)) value = stack.value;
        }
    }
    return value;
}

bool Buff::isStale() {
    return stacks.empty();
}

/* This will practically never do anything important, but it's here just in case */
void Buff::updateCallbacks(BuffCallback<int, BuffStack*> fOnUpdate, BuffCallback<time_t> fOnCombatTick) {
    if(!onUpdate) onUpdate = fOnUpdate;
    if(!onCombatTick) onCombatTick = fOnCombatTick;
}

#pragma region Handlers
void Buffs::timeBuffUpdate(EntityRef self, Buff* buff, int status, BuffStack* stack) {

    if(self.kind != EntityKind::PLAYER)
        return; // not implemented
    
    Player* plr = (Player*)self.getEntity();
    if(plr == nullptr)
        return; // sanity check

    if(status == ETBU_DEL && !buff->isStale())
        return; // no premature effect deletion

    int cbf = plr->getCompositeCondition();
    sTimeBuff payload{};
    if(status == ETBU_ADD) {
        payload.iValue = buff->getValue(BuffValueSelector::MAX_MAGNITUDE);
        // we need to explicitly add the ECSB for this buff,
        // in case this is the first stack in and the entry
        // in the buff map doesn't yet exist
        if(buff->id > 0) cbf |= CSB_FROM_ECSB(buff->id);
    }
    
    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
    pkt.eCSTB = buff->id; // eCharStatusTimeBuffID
    pkt.eTBU = status; // eTimeBuffUpdate
    pkt.eTBT = (int)stack->buffStackClass;
    pkt.iConditionBitFlag = cbf;
    pkt.TimeBuff = payload;
    self.sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

void Buffs::timeBuffTimeout(EntityRef self) {
    if(self.kind != EntityKind::PLAYER && self.kind != EntityKind::COMBAT_NPC && self.kind != EntityKind::MOB)
        return; // not a combatant
    Entity* entity = self.getEntity();
    ICombatant* combatant = dynamic_cast<ICombatant*>(entity);
    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt); // send a buff timeout to other players
    pkt.eCT = combatant->getCharType();
    pkt.iID = combatant->getID();
    pkt.iConditionBitFlag = combatant->getCompositeCondition();
    NPCManager::sendToViewable(entity, &pkt, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
}
#pragma endregion
