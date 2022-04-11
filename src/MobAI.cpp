#include "MobAI.hpp"
#include "Player.hpp"
#include "Racing.hpp"
#include "Transport.hpp"
#include "Nanos.hpp"
#include "Combat.hpp"
#include "Abilities.hpp"
#include "Rand.hpp"
#include "Items.hpp"
#include "Missions.hpp"

#include <cmath>
#include <limits.h>

using namespace MobAI;

bool MobAI::simulateMobs = settings::SIMULATEMOBS;

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
    mob->cbf = 0;
    mob->unbuffTimes.clear();

    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
    pkt1.eCT = 2;
    pkt1.iID = mob->id;
    pkt1.iConditionBitFlag = mob->cbf;
    NPCManager::sendToViewable(mob, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
}

void MobAI::followToCombat(Mob *mob) {
    if (NPCManager::NPCs.find(mob->groupLeader) != NPCManager::NPCs.end() && NPCManager::NPCs[mob->groupLeader]->kind == EntityType::MOB) {
        Mob* leadMob = (Mob*)NPCManager::NPCs[mob->groupLeader];
        for (int i = 0; i < 4; i++) {
            if (leadMob->groupMember[i] == 0)
                break;

            if (NPCManager::NPCs.find(leadMob->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[leadMob->groupMember[i]]->kind != EntityType::MOB) {
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
    if (NPCManager::NPCs.find(mob->groupLeader) == NPCManager::NPCs.end() || NPCManager::NPCs[mob->groupLeader]->kind != EntityType::MOB)
        return;

    Mob* leadMob = (Mob*)NPCManager::NPCs[mob->groupLeader];
    for (int i = 0; i < 4; i++) {
        if (leadMob->groupMember[i] == 0)
            break;

        if (NPCManager::NPCs.find(leadMob->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[leadMob->groupMember[i]]->kind != EntityType::MOB) {
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
            if (ref.type != EntityType::PLAYER)
                continue;

            CNSocket *s = ref.sock;
            Player *plr = PlayerManager::getPlayer(s);

            if (plr->HP <= 0 || plr->onMonkey)
                continue;

            int mobRange = mob->sightRange;

            if (plr->iConditionBitFlag & CSB_BIT_UP_STEALTH
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

static void dealCorruption(Mob *mob, std::vector<int> targetData, int skillID, int style) {
    Player *plr = PlayerManager::getPlayer(mob->target);

    size_t resplen = sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT) + targetData[0] * sizeof(sCAttackResult);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT), targetData[0], sizeof(sCAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_NPC_SKILL_CORRUPTION_HIT packet size" << std::endl;
        return;
    }

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_SKILL_CORRUPTION_HIT *resp = (sP_FE2CL_NPC_SKILL_CORRUPTION_HIT*)respbuf;
    sCAttackResult *respdata = (sCAttackResult*)(respbuf+sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT));

    resp->iNPC_ID = mob->id;
    resp->iSkillID = skillID;
    resp->iStyle = style;
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

        int style2 = Nanos::nanoStyle(plr->activeNano);
        if (style2 == -1) { // no nano
            respdata[i].iHitFlag = HF_BIT_STYLE_TIE;
            respdata[i].iDamage = Abilities::SkillTable[skillID].powerIntensity[0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
        } else if (style == style2) {
            respdata[i].iHitFlag = HF_BIT_STYLE_TIE;
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
        } else if (style - style2 == 1 || style2 - style == 2) {
            respdata[i].iHitFlag = HF_BIT_STYLE_WIN;
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina += 45;
            if (plr->Nanos[plr->activeNano].iStamina > 150)
                respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina = 150;
            // fire damage power disguised as a corruption attack back at the enemy
            std::vector<int> targetData2 = {1, mob->id, 0, 0, 0};
            for (auto& pwr : Abilities::Powers)
                if (pwr.skillType == EST_DAMAGE)
                    pwr.handle(sock, targetData2, plr->activeNano, skillID, 0, 200);
        } else {
            respdata[i].iHitFlag = HF_BIT_STYLE_LOSE;
            respdata[i].iDamage = Abilities::SkillTable[skillID].powerIntensity[0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina -= 90;
            if (plr->Nanos[plr->activeNano].iStamina < 0) {
                respdata[i].bNanoDeactive = 1;
                respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina = 0;
            }
        }

        if (!(plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE))
            plr->HP -= respdata[i].iDamage;

        respdata[i].iHP = plr->HP;
        respdata[i].iConditionBitFlag = plr->iConditionBitFlag;

        if (plr->HP <= 0) {
            if (!MobAI::aggroCheck(mob, getTime()))
                mob->transition(AIState::RETREAT, mob->target);
        }
    }

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_NPC_SKILL_CORRUPTION_HIT, resplen);
}

static void useAbilities(Mob *mob, time_t currTime) {
    /*
     * targetData approach
     * first integer is the count
     * second to fifth integers are IDs, these can be either player iID or mob's iID
     * whether the skill targets players or mobs is determined by the skill packet being fired
     */
    Player *plr = PlayerManager::getPlayer(mob->target);

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
        std::vector<int> targetData = {0, 0, 0, 0, 0};

        // find the players within range of eruption
        for (auto it = mob->viewableChunks.begin(); it != mob->viewableChunks.end(); it++) {
            Chunk* chunk = *it;
            for (const EntityRef& ref : chunk->entities) {
                // TODO: see aggroCheck()
                if (ref.type != EntityType::PLAYER)
                    continue;

                CNSocket *s= ref.sock;
                Player *plr = PlayerManager::getPlayer(s);

                if (plr->HP <= 0)
                    continue;

                int distance = hypot(mob->hitX - plr->x, mob->hitY - plr->y);
                if (distance < Abilities::SkillTable[skillID].effectArea) {
                    targetData[0] += 1;
                    targetData[targetData[0]] = plr->iID;
                    if (targetData[0] > 3) // make sure not to have more than 4
                        break;
                }
            }
        }

        for (auto& pwr : Abilities::Powers)
            if (pwr.skillType == Abilities::SkillTable[skillID].skillType)
                pwr.handle(mob->id, targetData, skillID, Abilities::SkillTable[skillID].durationTime[0], Abilities::SkillTable[skillID].powerIntensity[0]);
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
        std::vector<int> targetData = {1, plr->iID, 0, 0, 0};
        for (auto& pwr : Abilities::Powers)
            if (pwr.skillType == Abilities::SkillTable[skillID].skillType) {
                if (pwr.bitFlag != 0 && (plr->iConditionBitFlag & pwr.bitFlag))
                    return; // prevent debuffing a player twice
                pwr.handle(mob->id, targetData, skillID, Abilities::SkillTable[skillID].durationTime[0], Abilities::SkillTable[skillID].powerIntensity[0]);
            }
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

static void drainMobHP(Mob *mob, int amount) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_Damage);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_Damage *drain = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

    pkt->iID = mob->id;
    pkt->eCT = 4; // mob
    pkt->iTB_ID = ECSB_BOUNDINGBALL;

    drain->eCT = 4;
    drain->iID = mob->id;
    drain->iDamage = amount;
    drain->iHP = mob->hp -= amount;

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);

    if (mob->hp <= 0)
        mob->transition(AIState::DEAD, mob->target);
}

void Mob::deadStep(time_t currTime) {
    // despawn the mob after a short delay
    if (killedTime != 0 && !despawned && currTime - killedTime > 2000) {
        despawned = true;

        INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);

        pkt.iNPC_ID = id;

        NPCManager::sendToViewable(this, &pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));

        // if it was summoned, mark it for removal
        if (summoned) {
            std::cout << "[INFO] Queueing killed summoned mob for removal" << std::endl;
            NPCManager::queueNPCRemoval(id);
            return;
        }

        // pre-set spawn coordinates if not marked for removal
        x = spawnX;
        y = spawnY;
        z = spawnZ;
    }

    // to guide their groupmates, group leaders still need to move despite being dead
    if (groupLeader == id)
        roamingStep(currTime);

    if (killedTime != 0 && currTime - killedTime < regenTime * 100)
        return;

    std::cout << "respawning mob " << id << " with HP = " << maxHealth << std::endl;

    hp = maxHealth;
    state = AIState::ROAMING;

    // if mob is a group leader/follower, spawn where the group is.
    if (groupLeader != 0) {
        if (NPCManager::NPCs.find(groupLeader) != NPCManager::NPCs.end() && NPCManager::NPCs[groupLeader]->kind == EntityType::MOB) {
            Mob* leaderMob = (Mob*)NPCManager::NPCs[groupLeader];
            x = leaderMob->x + offsetX;
            y = leaderMob->y + offsetY;
            z = leaderMob->z;
        } else {
            std::cout << "[WARN] deadStep: mob cannot find it's leader!" << std::endl;
        }
    }

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = getAppearanceData();

    // notify all nearby players
    NPCManager::sendToViewable(this, &pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
}

void Mob::combatStep(time_t currTime) {
    assert(target != nullptr);

    // lose aggro if the player lost connection
    if (PlayerManager::players.find(target) == PlayerManager::players.end()) {
        if (!MobAI::aggroCheck(this, getTime()))
            transition(AIState::RETREAT, target);
        return;
    }

    Player *plr = PlayerManager::getPlayer(target);

    // lose aggro if the player became invulnerable or died
    if (plr->HP <= 0
     || (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE)) {
        if (!MobAI::aggroCheck(this, getTime()))
            transition(AIState::RETREAT, target);
        return;
    }

    // drain
    if (skillStyle < 0 && (lastDrainTime == 0 || currTime - lastDrainTime >= 1000)
        && cbf & CSB_BIT_BOUNDINGBALL) {
        drainMobHP(this, maxHealth / 20); // lose 5% every second
        lastDrainTime = currTime;
    }

    // if drain killed the mob, return early
    if (hp <= 0)
        return;

    // unbuffing
    std::unordered_map<int32_t, time_t>::iterator it = unbuffTimes.begin();
    while (it != unbuffTimes.end()) {

        if (currTime >= it->second) {
            cbf &= ~it->first;

            INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
            pkt1.eCT = 2;
            pkt1.iID = id;
            pkt1.iConditionBitFlag = cbf;
            NPCManager::sendToViewable(this, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));

            it = unbuffTimes.erase(it);
        } else {
            it++;
        }
    }

    // skip attack if stunned or asleep
    if (cbf & (CSB_BIT_STUN|CSB_BIT_MEZ)) {
        skillStyle = -1; // in this case we also reset the any outlying abilities the mob might be winding up.
        return;
    }

    int distance = hypot(plr->x - x, plr->y - y);
    int mobRange = (int)data["m_iAtkRange"] + (int)data["m_iRadius"];

    if (currTime >= nextAttack) {
        if (skillStyle != -1 || distance <= mobRange || Rand::rand(20) == 0) // while not in attack range, 1 / 20 chance.
            useAbilities(this, currTime);
        if (target == nullptr)
            return;
    }

    int distanceToTravel = INT_MAX;
    // movement logic: move when out of range but don't move while casting a skill
    if (distance > mobRange && skillStyle == -1) {
        if (nextMovement != 0 && currTime < nextMovement)
            return;
        nextMovement = currTime + 400;
        if (currTime >= nextAttack)
            nextAttack = 0;

        // halve movement speed if snared
        if (cbf & CSB_BIT_DN_MOVE_SPEED)
            speed /= 2;

        int targetX = plr->x;
        int targetY = plr->y;
        if (groupLeader != 0) {
            targetX += offsetX*distance/(idleRange + 1);
            targetY += offsetY*distance/(idleRange + 1);
        }

        distanceToTravel = std::min(distance-mobRange+1, speed*2/5);
        auto targ = lerp(x, y, targetX, targetY, distanceToTravel);
        if (distanceToTravel < speed*2/5 && currTime >= nextAttack)
            nextAttack = 0;

        NPCManager::updateNPCPosition(id, targ.first, targ.second, z, instanceID, angle);

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = id;
        pkt.iSpeed = speed;
        pkt.iToX = x = targ.first;
        pkt.iToY = y = targ.second;
        pkt.iToZ = plr->z;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(this, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    /* attack logic
     * 2/5 represents 400 ms which is the time interval mobs use per movement logic step
     * if the mob is one move interval away, we should just start attacking anyways.
     */
    if (distance <= mobRange || distanceToTravel < speed*2/5) {
        if (nextAttack == 0 || currTime >= nextAttack) {
            nextAttack = currTime + (int)data["m_iDelayTime"] * 100;
            Combat::npcAttackPc(this, currTime);
        }
    }

    // retreat if the player leaves combat range
    int xyDistance = hypot(plr->x - roamX, plr->y - roamY);
    distance = hypot(xyDistance, plr->z - roamZ);
    if (distance >= data["m_iCombatRange"]) {
        transition(AIState::RETREAT, target);
    }
}

void MobAI::incNextMovement(Mob *mob, time_t currTime) {
    if (currTime == 0)
        currTime = getTime();

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay/2 + Rand::rand(delay/2);
}

void Mob::roamingStep(time_t currTime) {
    /*
     * We reuse nextAttack to avoid scanning for players all the time, but to still
     * do so more often than if we waited for nextMovement (which is way too slow).
     * In the case of group leaders, this step will be called by dead mobs, so disable attack.
     */
    if (state != AIState::DEAD && (nextAttack == 0 || currTime >= nextAttack)) {
        nextAttack = currTime + 500;
        if (aggroCheck(this, currTime))
            return;
    }

    // no random roaming if the mob already has a set path
    if (staticPath)
        return;

    if (groupLeader != 0 && groupLeader != id) // don't roam by yourself without group leader
        return;

    /*
     * mob->nextMovement is also updated whenever the path queue is traversed in
     * Transport::stepNPCPathing() (which ticks at a higher frequency than nextMovement),
     * so we don't have to check if there's already entries in the queue since we know there won't be.
     */
    if (nextMovement != 0 && currTime < nextMovement)
        return;
    incNextMovement(this, currTime);

    int xStart = spawnX - idleRange/2;
    int yStart = spawnY - idleRange/2;

    // some mobs don't move (and we mustn't divide/modulus by zero)
    if (idleRange == 0 || speed == 0)
        return;

    int farX, farY, distance;
    int minDistance = idleRange / 2;

    // pick a random destination
    farX = xStart + Rand::rand(idleRange);
    farY = yStart + Rand::rand(idleRange);

    distance = std::abs(std::max(farX - x, farY - y));
    if (distance == 0)
        distance += 1; // hack to avoid FPE

    // if it's too short a walk, go further in that direction
    farX = x + (farX - x) * minDistance / distance;
    farY = y + (farY - y) * minDistance / distance;

    // but don't got out of bounds
    farX = std::clamp(farX, xStart, xStart + idleRange);
    farY = std::clamp(farY, yStart, yStart + idleRange);

    // halve movement speed if snared
    if (cbf & CSB_BIT_DN_MOVE_SPEED)
        speed /= 2;

    std::queue<Vec3> queue;
    Vec3 from = { x, y, z };
    Vec3 to = { farX, farY, z };

    // add a route to the queue; to be processed in Transport::stepNPCPathing()
    Transport::lerp(&queue, from, to, speed);
    Transport::NPCQueues[id] = queue;

    if (groupLeader != 0 && groupLeader == id) {
        // make followers follow this npc.
        for (int i = 0; i < 4; i++) {
            if (groupMember[i] == 0)
                break;

            if (NPCManager::NPCs.find(groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[groupMember[i]]->kind != EntityType::MOB) {
                std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
                continue;
            }

            std::queue<Vec3> queue2;
            Mob* followerMob = (Mob*)NPCManager::NPCs[groupMember[i]];
            from = { followerMob->x, followerMob->y, followerMob->z };
            to = { farX + followerMob->offsetX, farY + followerMob->offsetY, followerMob->z };
            Transport::lerp(&queue2, from, to, speed);
            Transport::NPCQueues[followerMob->id] = queue2;
        }
    }
}

void Mob::retreatStep(time_t currTime) {
    if (nextMovement != 0 && currTime < nextMovement)
        return;

    nextMovement = currTime + 400;

    // distance between spawn point and current location
    int distance = hypot(x - roamX, y - roamY);

    //if (distance > mob->data["m_iIdleRange"]) {
    if (distance > 10) {
        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        auto targ = lerp(x, y, roamX, roamY, (int)speed*4/5);

        pkt.iNPC_ID = id;
        pkt.iSpeed = (int)speed * 2;
        pkt.iToX = x = targ.first;
        pkt.iToY = y = targ.second;
        pkt.iToZ = z = spawnZ;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(this, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // if we got there
    //if (distance <= mob->data["m_iIdleRange"]) {
    if (distance <= 10) { // retreat back to the spawn point
        state = AIState::ROAMING;
        hp = maxHealth;
        killedTime = 0;
        nextAttack = 0;
        cbf = 0;

        // cast a return home heal spell, this is the right way(tm)
        std::vector<int> targetData = {1, 0, 0, 0, 0};
        for (auto& pwr : Abilities::Powers)
            if (pwr.skillType == Abilities::SkillTable[110].skillType)
                pwr.handle(id, targetData, 110, Abilities::SkillTable[110].durationTime[0], Abilities::SkillTable[110].powerIntensity[0]);
        // clear outlying debuffs
        clearDebuff(this);
    }
}

void Mob::onInactive() {
    // no-op
}

void Mob::onRoamStart() {
    // stub
}

void Mob::onCombatStart(EntityRef src) {
    assert(src.type == EntityType::PLAYER);
    target = src.sock;
    nextMovement = getTime();
    nextAttack = 0;

    roamX = x;
    roamY = y;
    roamZ = z;

    int skillID = (int)data["m_iPassiveBuff"]; // cast passive
    std::vector<int> targetData = { 1, id, 0, 0, 0 };
    for (auto& pwr : Abilities::Powers)
        if (pwr.skillType == Abilities::SkillTable[skillID].skillType)
            pwr.handle(id, targetData, skillID, Abilities::SkillTable[skillID].durationTime[0], Abilities::SkillTable[skillID].powerIntensity[0]);
}

void Mob::onRetreat() {
    target = nullptr;
    MobAI::clearDebuff(this);
    if (groupLeader != 0)
        MobAI::groupRetreat(this);
}

void Mob::onDeath(EntityRef src) {
    target = nullptr;
    cbf = 0;
    skillStyle = -1;
    unbuffTimes.clear();
    killedTime = getTime(); // XXX: maybe introduce a shard-global time for each step?

    // check for the edge case where hitting the mob did not aggro it
    if (src.type == EntityType::PLAYER && src.isValid()) {
        Player* plr = PlayerManager::getPlayer(src.sock);

        Items::DropRoll rolled;
        Items::DropRoll eventRolled;
        std::map<int, int> qitemRolls;

        Player* leader = PlayerManager::getPlayerFromID(plr->iIDGroup);
        assert(leader != nullptr); // should never happen

        Combat::genQItemRolls(leader, qitemRolls);

        if (plr->groupCnt == 1 && plr->iIDGroup == plr->iID) {
            Items::giveMobDrop(src.sock, this, rolled, eventRolled);
            Missions::mobKilled(src.sock, type, qitemRolls);
        }
        else {
            for (int i = 0; i < leader->groupCnt; i++) {
                CNSocket* sockTo = PlayerManager::getSockFromID(leader->groupIDs[i]);
                if (sockTo == nullptr)
                    continue;

                Player* otherPlr = PlayerManager::getPlayer(sockTo);

                // only contribute to group members' kills if they're close enough
                int dist = std::hypot(plr->x - otherPlr->x + 1, plr->y - otherPlr->y + 1);
                if (dist > 5000)
                    continue;

                Items::giveMobDrop(sockTo, this, rolled, eventRolled);
                Missions::mobKilled(sockTo, type, qitemRolls);
            }
        }
    }

    // delay the despawn animation
    despawned = false;

    auto it = Transport::NPCQueues.find(id);
    if (it == Transport::NPCQueues.end() || it->second.empty())
        return;

    // rewind or empty the movement queue
    if (staticPath) {
        /*
         * This is inelegant, but we wind forward in the path until we find the point that
         * corresponds with the Mob's spawn point.
         *
         * IMPORTANT: The check in TableData::loadPaths() must pass or else this will loop forever.
         */
        auto& queue = it->second;
        for (auto point = queue.front(); point.x != spawnX || point.y != spawnY; point = queue.front()) {
            queue.pop();
            queue.push(point);
        }
    }
    else {
        Transport::NPCQueues.erase(id);
    }
}
