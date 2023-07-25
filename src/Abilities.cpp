#include "Abilities.hpp"

#include "servers/CNShardServer.hpp"

#include "NPCManager.hpp"
#include "PlayerManager.hpp"
#include "Buffs.hpp"
#include "Nanos.hpp"
#include "MobAI.hpp"

using namespace Abilities;

std::map<int32_t, SkillData> Abilities::SkillTable;

#pragma region Skill handlers
static SkillResult handleSkillDamage(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    EntityRef sourceRef = source->getRef();
    double scalingFactor = 1;
    if(sourceRef.kind == EntityKind::PLAYER)
        scalingFactor = std::max(source->getMaxHP(), target->getMaxHP()) / 1000.0;
    else
        scalingFactor = source->getMaxHP() / 1500.0;

    int damage = (int)(skill->values[0][power] * scalingFactor);
    int dealt = target->takeDamage(sourceRef, damage);

    sSkillResult_Damage result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = dealt <= 0;
    result.iDamage = dealt;
    result.iHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Damage), &result);
}

static SkillResult handleSkillHealHP(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    EntityRef sourceRef = source->getRef();
    int heal = skill->values[0][power];
    double scalingFactor = target->getMaxHP() / 1000.0;
    int healed = target->heal(sourceRef, heal * scalingFactor);

    sSkillResult_Heal_HP result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iHealHP = healed;
    result.iHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Heal_HP), &result);
}

static SkillResult handleSkillDamageNDebuff(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    // take aggro
    target->takeDamage(source->getRef(), 0);

    int duration = 0;
    int strength = 0;
    bool blocked = target->hasBuff(ECSB_FREEDOM);
    if(!blocked) {
        duration = skill->durationTime[power];
        strength = skill->values[0][power];
        BuffStack debuff = {
            (duration * 100) / MS_PER_COMBAT_TICK, // ticks
            strength, // value
            source->getRef(), // source
            BuffClass::NANO, // buff class
        };
        int timeBuffId = Abilities::getCSTBFromST(skill->skillType);
        target->addBuff(timeBuffId,
            [](EntityRef self, Buff* buff, int status, BuffStack* stack) {
                Buffs::timeBuffUpdate(self, buff, status, stack);
                if(status == ETBU_DEL) Buffs::timeBuffTimeout(self);
            },
            [](EntityRef self, Buff* buff, time_t currTime) {
                Buffs::timeBuffTick(self, buff);
            },
            &debuff);
    }

    sSkillResult_Damage_N_Debuff result{};
    result.iDamage = duration / 10; // we use the duration as the damage number (why?)
    result.iHP = target->getCurrentHP();
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = blocked;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_Damage_N_Debuff), &result);
}

static SkillResult handleSkillLeech(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    EntityRef sourceRef = source->getRef();
    int heal = skill->values[0][power];
    int healed = source->heal(sourceRef, heal);
    int damage = heal * 2;
    int dealt = target->takeDamage(sourceRef, damage);

    sSkillResult_Leech result{};

    result.Damage.eCT = target->getCharType();
    result.Damage.iID = target->getID();
    result.Damage.bProtected = dealt <= 0;
    result.Damage.iDamage = dealt;
    result.Damage.iHP = target->getCurrentHP();

    result.Heal.eCT = result.Damage.eCT;
    result.Heal.iID = result.Damage.iID;
    result.Heal.iHealHP = healed;
    result.Heal.iHP = source->getCurrentHP();

    return SkillResult(sizeof(sSkillResult_Leech), &result);
}

static SkillResult handleSkillBuff(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    int duration = skill->durationTime[power];
    int strength = skill->values[0][power];
    BuffStack passiveBuff = {
        skill->drainType == SkillDrainType::PASSIVE ? 1 : (duration * 100) / MS_PER_COMBAT_TICK, // ticks
        strength, // value
        source->getRef(), // source
        source == target ? BuffClass::NANO : BuffClass::GROUP_NANO, // buff class
    };

    int timeBuffId = Abilities::getCSTBFromST(skill->skillType);
    SkillDrainType drainType = skill->drainType;
    if(!target->addBuff(timeBuffId,
        [drainType](EntityRef self, Buff* buff, int status, BuffStack* stack) {
            if(buff->id == ECSB_BOUNDINGBALL) {
                // drain
                ICombatant* combatant = dynamic_cast<ICombatant*>(self.getEntity());
                combatant->takeDamage(buff->getLastSource(), 0); // aggro
            }
            if(drainType == SkillDrainType::ACTIVE && status == ETBU_DEL)
                Buffs::timeBuffTimeout(self);
        },
        [](EntityRef self, Buff* buff, time_t currTime) {
            if(buff->id == ECSB_BOUNDINGBALL)
                Buffs::tickDrain(self, buff); // drain
        },
        &passiveBuff)) return SkillResult(); // no result if already buffed

    sSkillResult_Buff result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = false;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_Buff), &result);
}

static SkillResult handleSkillBatteryDrain(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    if(target->getCharType() != 1)
        return SkillResult(); // only Players are valid targets for battery drain
    Player* plr = dynamic_cast<Player*>(target);

    const double scalingFactor = (18 + source->getLevel()) / 36.0;

    int boostDrain = (int)(skill->values[0][power] * scalingFactor);
    if(boostDrain > plr->batteryW) boostDrain = plr->batteryW;
    plr->batteryW -= boostDrain;

    int potionDrain = (int)(skill->values[1][power] * scalingFactor);
    if(potionDrain > plr->batteryN) potionDrain = plr->batteryN;
    plr->batteryN -= potionDrain;

    sSkillResult_BatteryDrain result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.bProtected = target->hasBuff(ECSB_PROTECT_BATTERY);
    result.iDrainW = boostDrain;
    result.iBatteryW = plr->batteryW;
    result.iDrainN = potionDrain;
    result.iBatteryN = plr->batteryN;
    result.iStamina = plr->getActiveNano()->iStamina;
    result.bNanoDeactive = plr->getActiveNano()->iStamina <= 0;
    result.iConditionBitFlag = target->getCompositeCondition();
    return SkillResult(sizeof(sSkillResult_BatteryDrain), &result);
}

static SkillResult handleSkillMove(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    if(source->getCharType() != 1)
        return SkillResult(); // only Players are valid sources for recall

    Player* plr = dynamic_cast<Player*>(source);
    if(source == target) {
        // no trailing struct for self
        PlayerManager::sendPlayerTo(target->getRef().sock, plr->recallX, plr->recallY, plr->recallZ, plr->recallInstance);
        return SkillResult();
    }

    sSkillResult_Move result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iMapNum = plr->recallInstance;
    result.iMoveX = plr->recallX;
    result.iMoveY = plr->recallY;
    result.iMoveZ = plr->recallZ;
    return SkillResult(sizeof(sSkillResult_Move), &result);
}

static SkillResult handleSkillResurrect(SkillData* skill, int power, ICombatant* source, ICombatant* target) {
    sSkillResult_Resurrect result{};
    result.eCT = target->getCharType();
    result.iID = target->getID();
    result.iRegenHP = target->getCurrentHP();
    return SkillResult(sizeof(sSkillResult_Resurrect), &result);
}
#pragma endregion

static std::vector<SkillResult> handleSkill(SkillData* skill, int power, ICombatant* src, std::vector<ICombatant*> targets) {
    SkillResult (*skillHandler)(SkillData*, int, ICombatant*, ICombatant*) = nullptr;
    std::vector<SkillResult> results;

    switch(skill->skillType)
    {
        case SkillType::DAMAGE:
            skillHandler = handleSkillDamage;
            break;
        case SkillType::HEAL_HP:
        case SkillType::RETURNHOMEHEAL:
            skillHandler = handleSkillHealHP;
            break;
        case SkillType::KNOCKDOWN:
        case SkillType::SLEEP:
        case SkillType::SNARE:
        case SkillType::STUN:
            skillHandler = handleSkillDamageNDebuff;
            break;
        case SkillType::JUMP:
        case SkillType::RUN:
        case SkillType::STEALTH:
        case SkillType::MINIMAPENEMY:
        case SkillType::MINIMAPTRESURE:
        case SkillType::PHOENIX:
        case SkillType::PROTECTBATTERY:
        case SkillType::PROTECTINFECTION:
        case SkillType::REWARDBLOB:
        case SkillType::REWARDCASH:
        // case SkillType::INFECTIONDAMAGE:
        case SkillType::FREEDOM:
        case SkillType::BOUNDINGBALL:
        case SkillType::INVULNERABLE:
        case SkillType::STAMINA_SELF:
        case SkillType::NANOSTIMPAK:
        case SkillType::BUFFHEAL:
            skillHandler = handleSkillBuff;
            break;
        case SkillType::BLOODSUCKING:
            skillHandler = handleSkillLeech;
            break;
        case SkillType::RETROROCKET_SELF:
            // no-op
            return results;
        case SkillType::PHOENIX_GROUP:
            skillHandler = handleSkillResurrect;
            break;
        case SkillType::RECALL:
        case SkillType::RECALL_GROUP:
            skillHandler = handleSkillMove;
            break;
        case SkillType::BATTERYDRAIN:
            skillHandler = handleSkillBatteryDrain;
            break;
        default:
            std::cout << "[WARN] Unhandled skill type " << (int)skill->skillType << std::endl;
            return results;
    }

    for(ICombatant* target : targets) {
        assert(target != nullptr);
        SkillResult result = skillHandler(skill, power, src != nullptr ? src : target, target);
        if(result.size == 0) continue; // skill not applicable
        if(result.size > MAX_SKILLRESULT_SIZE) {
            std::cout << "[WARN] bad skill result size for " << (int)skill->skillType << " from " << (void*)handleSkillBuff << std::endl;
            continue;
        }
        results.push_back(result);
    }
    return results;
}

static void attachSkillResults(std::vector<SkillResult> results, uint8_t* pivot) {
    for(SkillResult& result : results) {
        size_t sz = result.size;
        memcpy(pivot, result.payload, sz);
        pivot += sz;
    }
}

void Abilities::useNanoSkill(CNSocket* sock, SkillData* skill, sNano& nano, std::vector<ICombatant*> affected) {

    Player* plr = PlayerManager::getPlayer(sock);
    ICombatant* combatant = dynamic_cast<ICombatant*>(plr);

    int boost = 0;
    if (Nanos::getNanoBoost(plr))
        boost = 3;

    if(skill->drainType == SkillDrainType::ACTIVE) {
        nano.iStamina -= skill->batteryUse[boost];
        if (nano.iStamina <= 0)
            nano.iStamina = 0;
    }

    std::vector<SkillResult> results = handleSkill(skill, boost, combatant, affected);
    if(results.empty()) return; // no effect; no need for confirmation packets

    // lazy validation since skill results might be different sizes
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), results.size(), MAX_SKILLRESULT_SIZE)) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC);
    for(SkillResult& sr : results)
        resplen += sr.size;
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE_SUCC* pkt = (sP_FE2CL_NANO_SKILL_USE_SUCC*)respbuf;
    pkt->iPC_ID = plr->iID;
    pkt->iNanoID = nano.iID;
    pkt->iSkillID = nano.iSkillID;
    pkt->iNanoStamina = nano.iStamina;
    pkt->bNanoDeactive = nano.iStamina <= 0;
    pkt->eST = (int32_t)skill->skillType;
    pkt->iTargetCnt = (int32_t)results.size();

    attachSkillResults(results, (uint8_t*)(pkt + 1));
    sock->sendPacket(pkt, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);

    if(skill->skillType == SkillType::RECALL_GROUP)
        // group recall packet is sent only to group members
        PlayerManager::sendToGroup(sock, pkt, P_FE2CL_NANO_SKILL_USE, resplen);
    else
        PlayerManager::sendToViewable(sock, pkt, P_FE2CL_NANO_SKILL_USE, resplen);
}

void Abilities::useNPCSkill(EntityRef npc, int skillID, std::vector<ICombatant*> affected) {
    if(SkillTable.count(skillID) == 0)
        return;

    Entity* entity = npc.getEntity();
    ICombatant* src = nullptr;
    if(npc.kind == EntityKind::COMBAT_NPC || npc.kind == EntityKind::MOB)
        src = dynamic_cast<ICombatant*>(entity);
    
    SkillData* skill = &SkillTable[skillID];

    std::vector<SkillResult> results = handleSkill(skill, 0, src, affected);
    if(results.empty()) return; // no effect; no need for confirmation packets

    // lazy validation since skill results might be different sizes
    if (!validOutVarPacket(sizeof(sP_FE2CL_NPC_SKILL_HIT), results.size(), MAX_SKILLRESULT_SIZE)) {
        std::cout << "[WARN] bad sP_FE2CL_NPC_SKILL_HIT packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT);
    for(SkillResult& sr : results)
        resplen += sr.size;
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_SKILL_HIT* pkt = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;
    pkt->iNPC_ID = npc.id;
    pkt->iSkillID = skillID;
    pkt->eST = (int32_t)skill->skillType;
    pkt->iTargetCnt = (int32_t)results.size();
    if(npc.kind == EntityKind::MOB) {
        Mob* mob = dynamic_cast<Mob*>(entity);
        pkt->iValue1 = mob->hitX;
        pkt->iValue2 = mob->hitY;
        pkt->iValue3 = mob->hitZ;
    }

    attachSkillResults(results, (uint8_t*)(pkt + 1));
    NPCManager::sendToViewable(entity, pkt, P_FE2CL_NPC_SKILL_HIT, resplen);
}

static std::vector<ICombatant*> entityRefsToCombatants(std::vector<EntityRef> refs) {
    std::vector<ICombatant*> combatants;
    for(EntityRef ref : refs) {
        if(ref.kind == EntityKind::PLAYER)
            combatants.push_back(dynamic_cast<ICombatant*>(PlayerManager::getPlayer(ref.sock)));
        else if(ref.kind == EntityKind::COMBAT_NPC || ref.kind == EntityKind::MOB)
            combatants.push_back(dynamic_cast<ICombatant*>(ref.getEntity()));
    }
    return combatants;
}

std::vector<ICombatant*> Abilities::matchTargets(ICombatant* src, SkillData* skill, int count, int32_t *ids) {

    if(skill->targetType == SkillTargetType::GROUP)
        return entityRefsToCombatants(src->getGroupMembers());

    // this check *has* to happen after the group check above due to cases like group recall that use both
    if(skill->effectTarget == SkillEffectTarget::SELF)
        return {src}; // client sends 0 targets for certain self-targeting skills (recall)

    // individuals
    std::vector<ICombatant*> targets;
    for (int i = 0; i < count; i++) {
        int32_t id = ids[i];
        if (skill->targetType == SkillTargetType::MOBS) {
            // mob
            if (NPCManager::NPCs.find(id) != NPCManager::NPCs.end()) {
                BaseNPC* npc = NPCManager::NPCs[id];
                if (npc->kind == EntityKind::COMBAT_NPC || npc->kind == EntityKind::MOB) {
                    targets.push_back(dynamic_cast<ICombatant*>(npc));
                    continue;
                }
            }
            std::cout << "[WARN] skill: invalid mob target (id " << id << ")\n";
        } else if(skill->targetType == SkillTargetType::PLAYERS) {
            // player
            Player* plr = PlayerManager::getPlayerFromID(id);
            if (plr != nullptr) {
                targets.push_back(dynamic_cast<ICombatant*>(plr));
                continue;
            }
            std::cout << "[WARN] skill: invalid player target (id " << id << ")\n";
        }
    }

    return targets; 
}

/* ripped from client (enums emplaced) */
int Abilities::getCSTBFromST(SkillType skillType) {
    int result = 0;
    switch (skillType)
    {
    case SkillType::RUN:
        result = ECSB_UP_MOVE_SPEED;
        break;
    case SkillType::JUMP:
        result = ECSB_UP_JUMP_HEIGHT;
        break;
    case SkillType::STEALTH:
        result = ECSB_UP_STEALTH;
        break;
    case SkillType::PHOENIX:
        result = ECSB_PHOENIX;
        break;
    case SkillType::PROTECTBATTERY:
        result = ECSB_PROTECT_BATTERY;
        break;
    case SkillType::PROTECTINFECTION:
        result = ECSB_PROTECT_INFECTION;
        break;
    case SkillType::MINIMAPENEMY:
        result = ECSB_MINIMAP_ENEMY;
        break;
    case SkillType::MINIMAPTRESURE:
        result = ECSB_MINIMAP_TRESURE;
        break;
    case SkillType::REWARDBLOB:
        result = ECSB_REWARD_BLOB;
        break;
    case SkillType::REWARDCASH:
        result = ECSB_REWARD_CASH;
        break;
    case SkillType::FREEDOM:
        result = ECSB_FREEDOM;
        break;
    case SkillType::INVULNERABLE:
        result = ECSB_INVULNERABLE;
        break;
    case SkillType::BUFFHEAL:
        result = ECSB_HEAL;
        break;
    case SkillType::NANOSTIMPAK:
        result = ECSB_STIMPAKSLOT1;
        // shift as necessary
        break;
    case SkillType::SNARE:
        result = ECSB_DN_MOVE_SPEED;
        break;
    case SkillType::STUN:
        result = ECSB_STUN;
        break;
    case SkillType::SLEEP:
        result = ECSB_MEZ;
        break;
    case SkillType::INFECTIONDAMAGE:
        result = ECSB_INFECTION;
        break;
    case SkillType::BOUNDINGBALL:
        result = ECSB_BOUNDINGBALL;
        break;
    default:
        break;
    }
    return result;
}
