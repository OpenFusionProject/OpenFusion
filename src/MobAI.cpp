#include "MobAI.hpp"

#include "Chunking.hpp"
#include "NPCManager.hpp"
#include "Entities.hpp"
#include "PlayerManager.hpp"
#include "Racing.hpp"
#include "Nanos.hpp"
#include "Abilities.hpp"
#include "Combat.hpp"
#include "Items.hpp"
#include "Missions.hpp"
#include "Rand.hpp"

#include <cmath>
#include <limits.h>

using namespace MobAI;

bool MobAI::simulateMobs = settings::SIMULATEMOBS;

void Mob::step(time_t currTime) {
    if (playersInView < 0)
        std::cout << "[WARN] Weird playerview value " << playersInView << std::endl;

    // skip movement and combat if disabled or not in view
    if ((!MobAI::simulateMobs || playersInView == 0) && state != AIState::DEAD
        && state != AIState::RETREAT)
        return;

    // call superclass step
    CombatNPC::step(currTime);
}

int Mob::takeDamage(EntityRef src, int amt) {

    // cannot kill mobs multiple times; cannot harm retreating mobs
    if (state != AIState::ROAMING && state != AIState::COMBAT) {
        return 0; // no damage
    }

    if (skillStyle >= 0)
        return 0; // don't hurt a mob casting corruption

    if (state == AIState::ROAMING) {
        if (target != nullptr || src.kind != EntityKind::PLAYER)
            return 0;
        transition(AIState::COMBAT, src);

        if (groupLeader != 0)
            MobAI::followToCombat(this);
    }

    // wake up sleeping monster
    if (hasBuff(ECSB_MEZ)) {
        removeBuff(ECSB_MEZ);

        INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
        pkt1.eCT = 2;
        pkt1.iID = id;
        pkt1.iConditionBitFlag = getCompositeCondition();
        NPCManager::sendToViewable(this, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
    }

    // call superclass takeDamage
    return CombatNPC::takeDamage(src, amt);
}

/*
 * Dynamic lerp; distinct from Transport::lerp(). This one doesn't care about height and
 * only returns the first step, since the rest will need to be recalculated anyway if chasing player.
 */
static std::pair<int,int> lerp(int x1, int y1, int x2, int y2, int speed) {
    std::pair<int,int> ret = {x1, y1};

    if (speed == 0)
        return ret;

    int distance = hypot(x1 - x2, y1 - y2);

    if (distance > speed) {

        int lerps = distance / speed;

        // interpolate only the first point
        float frac = 1.0f / lerps;

        ret.first = (x1 + (x2 - x1) * frac);
        ret.second = (y1 + (y2 - y1) * frac);
    } else {
        ret.first = x2;
        ret.second = y2;
    }

    return ret;
}

void MobAI::clearDebuff(Mob *mob) {
    mob->skillStyle = -1;
    mob->clearBuffs(false);

    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
    pkt1.eCT = 2;
    pkt1.iID = mob->id;
    pkt1.iConditionBitFlag = mob->getCompositeCondition();
    NPCManager::sendToViewable(mob, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
}

void MobAI::followToCombat(Mob *mob) {
    if (NPCManager::NPCs.find(mob->groupLeader) != NPCManager::NPCs.end() && NPCManager::NPCs[mob->groupLeader]->kind == EntityKind::MOB) {
        Mob* leadMob = (Mob*)NPCManager::NPCs[mob->groupLeader];
        for (int i = 0; i < 4; i++) {
            if (leadMob->groupMember[i] == 0)
                break;

            if (NPCManager::NPCs.find(leadMob->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[leadMob->groupMember[i]]->kind != EntityKind::MOB) {
                std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
                continue;
            }
            Mob* followerMob = (Mob*)NPCManager::NPCs[leadMob->groupMember[i]];

            if (followerMob->state != AIState::ROAMING) // only roaming mobs should transition to combat
                continue;

            followerMob->transition(AIState::COMBAT, mob->target);
        }

        if (leadMob->state != AIState::ROAMING)
            return;

        leadMob->transition(AIState::COMBAT, mob->target);
    }
}

void MobAI::groupRetreat(Mob *mob) {
    if (NPCManager::NPCs.find(mob->groupLeader) == NPCManager::NPCs.end() || NPCManager::NPCs[mob->groupLeader]->kind != EntityKind::MOB)
        return;

    Mob* leadMob = (Mob*)NPCManager::NPCs[mob->groupLeader];
    for (int i = 0; i < 4; i++) {
        if (leadMob->groupMember[i] == 0)
            break;

        if (NPCManager::NPCs.find(leadMob->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[leadMob->groupMember[i]]->kind != EntityKind::MOB) {
            std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
            continue;
        }
        Mob* followerMob = (Mob*)NPCManager::NPCs[leadMob->groupMember[i]];

        if (followerMob->state != AIState::COMBAT)
            continue;

        followerMob->target = nullptr;
        followerMob->state = AIState::RETREAT;
        clearDebuff(followerMob);
    }

    if (leadMob->state != AIState::COMBAT)
        return;

    leadMob->target = nullptr;
    leadMob->state = AIState::RETREAT;
    clearDebuff(leadMob);
}

/*
 * Aggro on nearby players.
 * Even if they're in range, we can't assume they're all in the same one chunk
 * as the mob, since it might be near a chunk boundary.
 */
bool MobAI::aggroCheck(Mob *mob, time_t currTime) {
    CNSocket *closest = nullptr;
    int closestDistance = INT_MAX;

    for (auto it = mob->viewableChunks.begin(); it != mob->viewableChunks.end(); it++) {
        Chunk* chunk = *it;
        for (const EntityRef& ref : chunk->entities) {
            // TODO: support targetting other CombatNPCs
            if (ref.kind != EntityKind::PLAYER)
                continue;

            CNSocket *s = ref.sock;
            Player *plr = PlayerManager::getPlayer(s);

            if (plr == nullptr) continue;
            if (plr->HP <= 0 || plr->onMonkey)
                continue;

            int mobRange = mob->sightRange;

            if (plr->hasBuff(ECSB_UP_STEALTH)
            || Racing::EPRaces.find(s) != Racing::EPRaces.end())
                mobRange /= 3;

            // 0.33x - 1.66x the range
            int levelDifference = plr->level - mob->level;
            if (levelDifference > -10)
                mobRange = levelDifference < 10 ? mobRange - (levelDifference * mobRange / 15) : mobRange / 3;

            if (mob->state != AIState::ROAMING && plr->inCombat) // freshly out of aggro mobs
                mobRange = mob->sightRange * 2; // should not be impacted by the above

            if (plr->iSpecialState & (CN_SPECIAL_STATE_FLAG__INVISIBLE|CN_SPECIAL_STATE_FLAG__INVULNERABLE))
                mobRange = -1;

            // height is relevant for aggro distance because of platforming
            int xyDistance = hypot(mob->x - plr->x, mob->y - plr->y);
            int distance = hypot(xyDistance, (mob->z - plr->z) * 2); // difference in Z counts twice

            if (distance > mobRange || distance > closestDistance)
                continue;

            // found a player
            closest = s;
            closestDistance = distance;
        }
    }

    if (closest != nullptr) {
        // found closest player. engage.
        mob->transition(AIState::COMBAT, closest);

        if (mob->groupLeader != 0)
            followToCombat(mob);

        return true;
    }

    return false;
}

static void dealCorruption(Mob *mob, std::vector<int> targetData, int skillID, int mobStyle) {
    Player *plr = PlayerManager::getPlayer(mob->target);
    if (plr == nullptr) return;

    size_t resplen = sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT) + targetData[0] * sizeof(sCAttackResult);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT), targetData[0], sizeof(sCAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_NPC_SKILL_CORRUPTION_HIT packet size" << std::endl;
        return;
    }

    uint8_t respbuf[CN_PACKET_BODY_SIZE];
    memset(respbuf, 0, CN_PACKET_BODY_SIZE);

    sP_FE2CL_NPC_SKILL_CORRUPTION_HIT *resp = (sP_FE2CL_NPC_SKILL_CORRUPTION_HIT*)respbuf;
    sCAttackResult *respdata = (sCAttackResult*)(respbuf+sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT));

    resp->iNPC_ID = mob->id;
    resp->iSkillID = skillID;
    resp->iStyle = mobStyle;
    resp->iValue1 = plr->x;
    resp->iValue2 = plr->y;
    resp->iValue3 = plr->z;
    resp->iTargetCnt = targetData[0];

    for (int i = 0; i < targetData[0]; i++) {
        CNSocket *sock = nullptr;
        Player *plr = nullptr;

        for (auto& pair : PlayerManager::players) {
            if (pair.second->iID == targetData[i+1]) {
                sock = pair.first;
                plr = pair.second;
                break;
            }
        }

        // player not found
        if (plr == nullptr) {
            std::cout << "[WARN] dealCorruption: player ID not found" << std::endl;
            return;
        }

        respdata[i].eCT = 1;
        respdata[i].iID = plr->iID;
        respdata[i].bProtected = 0;

        respdata[i].iActiveNanoSlotNum = -1;
        for (int n = 0; n < 3; n++)
            if (plr->activeNano == plr->equippedNanos[n])
                respdata[i].iActiveNanoSlotNum = n;
        respdata[i].iNanoID = plr->activeNano;

        int nanoStyle = Nanos::nanoStyle(plr->activeNano);
        if (nanoStyle == -1) { // no nano
            respdata[i].iHitFlag = HF_BIT_STYLE_TIE;
            respdata[i].iDamage = Abilities::SkillTable[skillID].values[0][0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
        } else if (mobStyle == nanoStyle) {
            respdata[i].iHitFlag = HF_BIT_STYLE_TIE;
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
        } else if (mobStyle - nanoStyle == 1 || nanoStyle - mobStyle == 2) {
            respdata[i].iHitFlag = HF_BIT_STYLE_WIN;
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina += 45;
            if (plr->Nanos[plr->activeNano].iStamina > 150)
                respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina = 150;
            // fire damage power disguised as a corruption attack back at the enemy
            SkillData skill = {
                SkillType::DAMAGE, // skillType
                SkillEffectTarget::POINT, // effectTarget
                1, // effectType
                SkillTargetType::MOBS, // targetType
                SkillDrainType::ACTIVE, // drainType
                0, // effectArea
                {0, 0, 0, 0}, // batteryUse
                {0, 0, 0, 0}, // durationTime
                {0, 0, 0}, // valueTypes (unused)
                {
                    {200, 200, 200, 200},
                    {200, 200, 200, 200},
                    {200, 200, 200, 200},
                }
            };
            Abilities::useNanoSkill(sock, &skill, *plr->getActiveNano(), { mob });
        } else {
            respdata[i].iHitFlag = HF_BIT_STYLE_LOSE;
            respdata[i].iDamage = Abilities::SkillTable[skillID].values[0][0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina -= 90;
            if (plr->Nanos[plr->activeNano].iStamina < 0) {
                respdata[i].bNanoDeactive = 1;
                respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina = 0;
            }
        }

        if (!(plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE))
            plr->HP -= respdata[i].iDamage;

        respdata[i].iHP = plr->HP;
        respdata[i].iConditionBitFlag = plr->getCompositeCondition();

        if (plr->HP <= 0) {
            if (!MobAI::aggroCheck(mob, getTime()))
                mob->transition(AIState::RETREAT, mob->target);
        }
    }

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_NPC_SKILL_CORRUPTION_HIT, resplen);
}

static void useAbilities(Mob *mob, time_t currTime) {
    Player *plr = PlayerManager::getPlayer(mob->target);
    if (plr == nullptr) return;

    if (mob->skillStyle >= 0) { // corruption hit
        int skillID = (int)mob->data["m_iCorruptionType"];
        std::vector<int> targetData = {1, plr->iID, 0, 0, 0};
        int temp = mob->skillStyle;
        mob->skillStyle = -3; // corruption cooldown
        mob->nextAttack = currTime + 1000;
        dealCorruption(mob, targetData, skillID, temp);
        return;
    }

    if (mob->skillStyle == -2) { // eruption hit
        int skillID = (int)mob->data["m_iMegaType"];
        std::vector<ICombatant*> targets{};

        // find the players within range of eruption
        for (auto it = mob->viewableChunks.begin(); it != mob->viewableChunks.end(); it++) {
            Chunk* chunk = *it;
            for (const EntityRef& ref : chunk->entities) {
                // TODO: see aggroCheck()
                if (ref.kind != EntityKind::PLAYER)
                    continue;

                CNSocket *s = ref.sock;
                Player *plr = PlayerManager::getPlayer(s);

                if (plr == nullptr) continue;
                if (!plr->isAlive())
                    continue;

                int distance = hypot(mob->hitX - plr->x, mob->hitY - plr->y);
                if (distance < Abilities::SkillTable[skillID].effectArea) {
                    targets.push_back(plr);
                    if (targets.size() > 3) // make sure not to have more than 4
                        break;
                }
            }
        }

        Abilities::useNPCSkill(mob->id, skillID, targets);
        mob->skillStyle = -3; // eruption cooldown
        mob->nextAttack = currTime + 1000;
        return;
    }

    if (mob->skillStyle == -3) { // cooldown expires
        mob->skillStyle = -1;
        return;
    }

    int random = Rand::rand(2000) * 1000;
    int prob1 = (int)mob->data["m_iActiveSkill1Prob"]; // active skill probability
    int prob2 = (int)mob->data["m_iCorruptionTypeProb"]; // corruption probability
    int prob3 = (int)mob->data["m_iMegaTypeProb"]; // eruption probability

    if (random < prob1) { // active skill hit
        int skillID = (int)mob->data["m_iActiveSkill1"];
        SkillData* skill = &Abilities::SkillTable[skillID];
        int debuffID = Abilities::getCSTBFromST(skill->skillType);
        if(plr->hasBuff(debuffID))
            return; // prevent debuffing a player twice
        Abilities::useNPCSkill(mob->getRef(), skillID, { plr });
        mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
        return;
    }

    if (random < prob1 + prob2) { // corruption windup
        int skillID = (int)mob->data["m_iCorruptionType"];
        INITSTRUCT(sP_FE2CL_NPC_SKILL_CORRUPTION_READY, pkt);
        pkt.iNPC_ID = mob->id;
        pkt.iSkillID = skillID;
        pkt.iValue1 = plr->x;
        pkt.iValue2 = plr->y;
        pkt.iValue3 = plr->z;
        mob->skillStyle = Nanos::nanoStyle(plr->activeNano) - 1;
        if (mob->skillStyle == -1)
            mob->skillStyle = 2;
        if (mob->skillStyle == -2)
            mob->skillStyle = Rand::rand(3);
        pkt.iStyle = mob->skillStyle;
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_SKILL_CORRUPTION_READY, sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_READY));
        mob->nextAttack = currTime + 1800;
        return;
    }

    if (random < prob1 + prob2 + prob3) { // eruption windup
        int skillID = (int)mob->data["m_iMegaType"];
        INITSTRUCT(sP_FE2CL_NPC_SKILL_READY, pkt);
        pkt.iNPC_ID = mob->id;
        pkt.iSkillID = skillID;
        pkt.iValue1 = mob->hitX = plr->x;
        pkt.iValue2 = mob->hitY = plr->y;
        pkt.iValue3 = mob->hitZ = plr->z;
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_SKILL_READY, sizeof(sP_FE2CL_NPC_SKILL_READY));
        mob->nextAttack = currTime + 1800;
        mob->skillStyle = -2;
        return;
    }

    return;
}

void MobAI::incNextMovement(Mob* mob, time_t currTime) {
    if (currTime == 0)
        currTime = getTime();

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay / 2 + Rand::rand(delay / 2);
}

void MobAI::deadStep(CombatNPC* npc, time_t currTime) {
    Mob* self = (Mob*)npc;

    // despawn the mob after a short delay
    if (self->killedTime != 0 && !self->despawned && currTime - self->killedTime > 2000) {
        self->despawned = true;

        INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);

        pkt.iNPC_ID = self->id;

        NPCManager::sendToViewable(self, &pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));

        // if it was summoned, mark it for removal
        if (self->summoned) {
            std::cout << "[INFO] Queueing killed summoned mob for removal" << std::endl;
            NPCManager::queueNPCRemoval(self->id);
            return;
        }

        // pre-set spawn coordinates if not marked for removal
        self->x = self->spawnX;
        self->y = self->spawnY;
        self->z = self->spawnZ;
    }

    // to guide their groupmates, group leaders still need to move despite being dead
    if (self->groupLeader == self->id)
        roamingStep(self, currTime);

    /*
     * If the mob hasn't fully despanwed yet, don't try to respawn it. This protects
     * against the edge case where mobs with a very short regenTime would try to respawn
     * before they've faded away; and would respawn even if they were meant to be removed.
     */
    if (!self->despawned)
        return;

    if (self->killedTime != 0 && currTime - self->killedTime < self->regenTime * 100)
        return;

    std::cout << "respawning mob " << self->id << " with HP = " << self->maxHealth << std::endl;

    self->transition(AIState::ROAMING, self->id);

    // if mob is a group leader/follower, spawn where the group is.
    if (self->groupLeader != 0) {
        if (NPCManager::NPCs.find(self->groupLeader) != NPCManager::NPCs.end() && NPCManager::NPCs[self->groupLeader]->kind == EntityKind::MOB) {
            Mob* leaderMob = (Mob*)NPCManager::NPCs[self->groupLeader];
            self->x = leaderMob->x + self->offsetX;
            self->y = leaderMob->y + self->offsetY;
            self->z = leaderMob->z;
        } else {
            std::cout << "[WARN] deadStep: mob cannot find it's leader!" << std::endl;
        }
    }

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = self->getAppearanceData();

    // notify all nearby players
    NPCManager::sendToViewable(self, &pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
}

void MobAI::combatStep(CombatNPC* npc, time_t currTime) {
    Mob* self = (Mob*)npc;
    if (self->target == nullptr) {
        self->transition(AIState::RETREAT, self->id);
        return;
    }

    // lose aggro if the player lost connection
    if (PlayerManager::players.find(self->target) == PlayerManager::players.end()) {
        if (!MobAI::aggroCheck(self, getTime()))
            self->transition(AIState::RETREAT, self->target);
        return;
    }

    Player *plr = PlayerManager::getPlayer(self->target);
    if (plr == nullptr) {
        self->transition(AIState::RETREAT, self->target);
        return;
    }

    // lose aggro if the player became invulnerable or died
    if (plr->HP <= 0
     || (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE)) {
        if (!MobAI::aggroCheck(self, getTime()))
            self->transition(AIState::RETREAT, self->target);
        return;
    }

    // tick buffs
    auto it = npc->buffs.begin();
    while(it != npc->buffs.end()) {
        Buff* buff = (*it).second;
        buff->combatTick(currTime);

        // if mob state changed, end the step
        if(self->state != AIState::COMBAT)
            return;

        buff->tick(currTime);
        if(buff->isStale()) {
            // garbage collect
            it = npc->buffs.erase(it);
            delete buff;
        }
        else it++;
    }

    // skip attack if stunned or asleep
    if (self->hasBuff(ECSB_STUN) || self->hasBuff(ECSB_MEZ)) {
        self->skillStyle = -1; // in this case we also reset the any outlying abilities the mob might be winding up.
        return;
    }

    int distance = hypot(plr->x - self->x, plr->y - self->y);
    int mobRange = (int)self->data["m_iAtkRange"] + (int)self->data["m_iRadius"];

    if (currTime >= self->nextAttack) {
        if (self->skillStyle != -1 || distance <= mobRange || Rand::rand(20) == 0) // while not in attack range, 1 / 20 chance.
            useAbilities(self, currTime);
        if (self->target == nullptr)
            return;
    }

    int distanceToTravel = INT_MAX;
    int speed = self->speed;
    // movement logic: move when out of range but don't move while casting a skill
    if (distance > mobRange && self->skillStyle == -1) {
        if (self->nextMovement != 0 && currTime < self->nextMovement)
            return;
        self->nextMovement = currTime + 400;
        if (currTime >= self->nextAttack)
            self->nextAttack = 0;

        // halve movement speed if snared
        if (self->hasBuff(ECSB_DN_MOVE_SPEED))
            speed /= 2;

        int targetX = plr->x;
        int targetY = plr->y;
        if (self->groupLeader != 0) {
            targetX += self->offsetX*distance/(self->idleRange + 1);
            targetY += self->offsetY*distance/(self->idleRange + 1);
        }

        distanceToTravel = std::min(distance-mobRange+1, speed*2/5);
        auto targ = lerp(self->x, self->y, targetX, targetY, distanceToTravel);
        if (distanceToTravel < speed*2/5 && currTime >= self->nextAttack)
            self->nextAttack = 0;

        NPCManager::updateNPCPosition(self->id, targ.first, targ.second, self->z, self->instanceID, self->angle);

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = self->id;
        pkt.iSpeed = speed;
        pkt.iToX = self->x = targ.first;
        pkt.iToY = self->y = targ.second;
        pkt.iToZ = plr->z;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(self, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    /* attack logic
     * 2/5 represents 400 ms which is the time interval mobs use per movement logic step
     * if the mob is one move interval away, we should just start attacking anyways.
     */
    if (distance <= mobRange || distanceToTravel < self->speed*2/5) {
        if (self->nextAttack == 0 || currTime >= self->nextAttack) {
            self->nextAttack = currTime + (int)self->data["m_iDelayTime"] * 100;
            Combat::npcAttackPc(self, currTime);
        }
    }

    // retreat if the player leaves combat range
    int xyDistance = hypot(plr->x - self->roamX, plr->y - self->roamY);
    distance = hypot(xyDistance, plr->z - self->roamZ);
    if (distance >= self->data["m_iCombatRange"]) {
        self->transition(AIState::RETREAT, self->target);
    }
}

void MobAI::roamingStep(CombatNPC* npc, time_t currTime) {
    Mob* self = (Mob*)npc;

    /*
     * We reuse nextAttack to avoid scanning for players all the time, but to still
     * do so more often than if we waited for nextMovement (which is way too slow).
     * In the case of group leaders, this step will be called by dead mobs, so disable attack.
     */
    if (self->state != AIState::DEAD && (self->nextAttack == 0 || currTime >= self->nextAttack)) {
        self->nextAttack = currTime + 500;
        if (aggroCheck(self, currTime))
            return;
    }

    // no random roaming if the mob already has a set path
    if (self->staticPath)
        return;

    if (self->groupLeader != 0 && self->groupLeader != self->id) // don't roam by yourself without group leader
        return;

    /*
     * mob->nextMovement is also updated whenever the path queue is traversed in
     * Transport::stepNPCPathing() (which ticks at a higher frequency than nextMovement),
     * so we don't have to check if there's already entries in the queue since we know there won't be.
     */
    if (self->nextMovement != 0 && currTime < self->nextMovement)
        return;
    incNextMovement(self, currTime);

    int xStart = self->spawnX - self->idleRange/2;
    int yStart = self->spawnY - self->idleRange/2;

    // some mobs don't move (and we mustn't divide/modulus by zero)
    if (self->idleRange == 0 || self->speed == 0)
        return;

    int farX, farY, distance;
    int minDistance = self->idleRange / 2;

    // pick a random destination
    farX = xStart + Rand::rand(self->idleRange);
    farY = yStart + Rand::rand(self->idleRange);

    distance = std::abs(std::max(farX - self->x, farY - self->y));
    if (distance == 0)
        distance += 1; // hack to avoid FPE

    // if it's too short a walk, go further in that direction
    farX = self->x + (farX - self->x) * minDistance / distance;
    farY = self->y + (farY - self->y) * minDistance / distance;

    // but don't got out of bounds
    farX = std::clamp(farX, xStart, xStart + self->idleRange);
    farY = std::clamp(farY, yStart, yStart + self->idleRange);

    // halve movement speed if snared
    if (self->hasBuff(ECSB_DN_MOVE_SPEED))
        self->speed /= 2;

    std::queue<Vec3> queue;
    Vec3 from = { self->x, self->y, self->z };
    Vec3 to = { farX, farY, self->z };

    // add a route to the queue; to be processed in Transport::stepNPCPathing()
    Transport::lerp(&queue, from, to, self->speed);
    Transport::NPCQueues[self->id] = queue;

    if (self->groupLeader != 0 && self->groupLeader == self->id) {
        // make followers follow this npc.
        for (int i = 0; i < 4; i++) {
            if (self->groupMember[i] == 0)
                break;

            if (NPCManager::NPCs.find(self->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[self->groupMember[i]]->kind != EntityKind::MOB) {
                std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
                continue;
            }

            std::queue<Vec3> queue2;
            Mob* followerMob = (Mob*)NPCManager::NPCs[self->groupMember[i]];
            from = { followerMob->x, followerMob->y, followerMob->z };
            to = { farX + followerMob->offsetX, farY + followerMob->offsetY, followerMob->z };
            Transport::lerp(&queue2, from, to, self->speed);
            Transport::NPCQueues[followerMob->id] = queue2;
        }
    }
}

void MobAI::retreatStep(CombatNPC* npc, time_t currTime) {
    Mob* self = (Mob*)npc;

    if (self->nextMovement != 0 && currTime < self->nextMovement)
        return;

    self->nextMovement = currTime + 400;

    // distance between spawn point and current location
    int distance = hypot(self->x - self->roamX, self->y - self->roamY);

    //if (distance > mob->data["m_iIdleRange"]) {
    if (distance > 10) {
        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        auto targ = lerp(self->x, self->y, self->roamX, self->roamY, (int)self->speed*4/5);

        pkt.iNPC_ID = self->id;
        pkt.iSpeed = (int)self->speed * 2;
        pkt.iToX = self->x = targ.first;
        pkt.iToY = self->y = targ.second;
        pkt.iToZ = self->z = self->spawnZ;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(self, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // if we got there
    //if (distance <= mob->data["m_iIdleRange"]) {
    if (distance <= 10) { // retreat back to the spawn point
        self->transition(AIState::ROAMING, self->id);
    }
}

void MobAI::onRoamStart(CombatNPC* npc, EntityRef src) {
    Mob* self = (Mob*)npc;

    self->hp = self->maxHealth;
    self->killedTime = 0;
    self->nextAttack = 0;

    // cast a return home heal spell, this is the right way(tm)
    Abilities::useNPCSkill(npc->getRef(), 110, { npc });

    // clear outlying debuffs
    clearDebuff(self);
}

void MobAI::onCombatStart(CombatNPC* npc, EntityRef src) {
    Mob* self = (Mob*)npc;

    if (src.kind != EntityKind::PLAYER)
        return;
    self->target = src.sock;
    self->nextMovement = getTime();
    self->nextAttack = 0;

    self->roamX = self->x;
    self->roamY = self->y;
    self->roamZ = self->z;

    int skillID = (int)self->data["m_iPassiveBuff"];
    if(skillID != 0) // cast passive
        Abilities::useNPCSkill(npc->getRef(), skillID, { npc });
}

void MobAI::onRetreat(CombatNPC* npc, EntityRef src) {
    Mob* self = (Mob*)npc;

    self->target = nullptr;
    MobAI::clearDebuff(self);
    if (self->groupLeader != 0)
        MobAI::groupRetreat(self);
}

void MobAI::onDeath(CombatNPC* npc, EntityRef src) {
    Mob* self = (Mob*)npc;

    self->target = nullptr;
    self->skillStyle = -1;
    self->clearBuffs(true);
    self->killedTime = getTime(); // XXX: maybe introduce a shard-global time for each step?

    // check for the edge case where hitting the mob did not aggro it
    if (src.kind == EntityKind::PLAYER && src.isValid()) {
        Player* plr = PlayerManager::getPlayer(src.sock);
        if (plr == nullptr) return;

        Items::DropRoll rolled;
        Items::DropRoll eventRolled;
        std::map<int, int> qitemRolls;
        std::vector<Player*> playerRefs;

        if (plr->group == nullptr) {
            playerRefs.push_back(plr);
            Combat::genQItemRolls(playerRefs, qitemRolls);
            Items::giveMobDrop(src.sock, self, rolled, eventRolled);
            Missions::mobKilled(src.sock, self->type, qitemRolls);
        }
        else {
            auto players = plr->group->filter(EntityKind::PLAYER);
            for (EntityRef pRef : players) {
                Player* pRefPlr = PlayerManager::getPlayer(pRef.sock);
                if (pRefPlr != nullptr) playerRefs.push_back(pRefPlr);
            }
            Combat::genQItemRolls(playerRefs, qitemRolls);
            for (int i = 0; i < players.size(); i++) {
                CNSocket* sockTo = players[i].sock;
                Player* otherPlr = PlayerManager::getPlayer(sockTo);

                if (otherPlr == nullptr) continue;
                // only contribute to group members' kills if they're close enough
                int dist = std::hypot(plr->x - otherPlr->x + 1, plr->y - otherPlr->y + 1);
                if (dist > 5000)
                    continue;

                Items::giveMobDrop(sockTo, self, rolled, eventRolled);
                Missions::mobKilled(sockTo, self->type, qitemRolls);
            }
        }
    }

    // delay the despawn animation
    self->despawned = false;

    auto it = Transport::NPCQueues.find(self->id);
    if (it == Transport::NPCQueues.end() || it->second.empty())
        return;

    // rewind or empty the movement queue
    if (self->staticPath) {
        /*
         * This is inelegant, but we wind forward in the path until we find the point that
         * corresponds with the Mob's spawn point.
         *
         * IMPORTANT: The check in TableData::loadPaths() must pass or else this will loop forever.
         */
        auto& queue = it->second;
        for (auto point = queue.front(); point.x != self->spawnX || point.y != self->spawnY; point = queue.front()) {
            queue.pop();
            queue.push(point);
        }
    }
    else {
        Transport::NPCQueues.erase(self->id);
    }
}
