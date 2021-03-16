#include "MobAI.hpp"
#include "Player.hpp"
#include "RacingManager.hpp"
#include "TransportManager.hpp"
#include "NanoManager.hpp"
#include "Combat.hpp"
#include "Abilities.hpp"

#include <cmath>
#include <limits.h>

using namespace MobAI;

std::map<int32_t, Mob*> MobAI::Mobs;
static std::queue<int32_t> RemovalQueue;

bool MobAI::simulateMobs;

static void roamingStep(Mob *mob, time_t currTime);

/*
 * Dynamic lerp; distinct from TransportManager::lerp(). This one doesn't care about height and
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
    mob->appearanceData.iConditionBitFlag = 0;
    mob->unbuffTimes.clear();

    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
    pkt1.eCT = 2;
    pkt1.iID = mob->appearanceData.iNPC_ID;
    pkt1.iConditionBitFlag = mob->appearanceData.iConditionBitFlag;
    NPCManager::sendToViewable(mob, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
}

void MobAI::followToCombat(Mob *mob) {
    if (Mobs.find(mob->groupLeader) != Mobs.end()) {
        Mob* leadMob = Mobs[mob->groupLeader];
        for (int i = 0; i < 4; i++) {
            if (leadMob->groupMember[i] == 0)
                break;

            if (Mobs.find(leadMob->groupMember[i]) == Mobs.end()) {
                std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
                continue;
            }
            Mob* followerMob = Mobs[leadMob->groupMember[i]];

            if (followerMob->state != MobState::ROAMING) // only roaming mobs should transition to combat
                continue;

            enterCombat(mob->target, followerMob);
        }

        if (leadMob->state != MobState::ROAMING)
            return;

        enterCombat(mob->target, leadMob);
    }
}

void MobAI::groupRetreat(Mob *mob) {
    if (Mobs.find(mob->groupLeader) == Mobs.end())
        return;

    Mob* leadMob = Mobs[mob->groupLeader];
    for (int i = 0; i < 4; i++) {
        if (leadMob->groupMember[i] == 0)
            break;

        if (Mobs.find(leadMob->groupMember[i]) == Mobs.end()) {
            std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
            continue;
        }
        Mob* followerMob = Mobs[leadMob->groupMember[i]];

        followerMob->target = nullptr;
        followerMob->state = MobState::RETREAT;
        clearDebuff(followerMob);
    }

    leadMob->target = nullptr;
    leadMob->state = MobState::RETREAT;
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

    for (auto it = mob->viewableChunks->begin(); it != mob->viewableChunks->end(); it++) {
        Chunk* chunk = *it;
        for (CNSocket *s : chunk->players) {
            Player *plr = PlayerManager::getPlayer(s);

            if (plr->HP <= 0)
                continue;

            int mobRange = mob->sightRange;

            if (plr->iConditionBitFlag & CSB_BIT_UP_STEALTH
            || RacingManager::EPRaces.find(s) != RacingManager::EPRaces.end())
                mobRange /= 3;

            // 0.33x - 1.66x the range
            int levelDifference = plr->level - mob->level;
            if (levelDifference > -10)
                mobRange = levelDifference < 10 ? mobRange - (levelDifference * mobRange / 15) : mobRange / 3;

            if (mob->state != MobState::ROAMING && plr->inCombat) // freshly out of aggro mobs
                mobRange = mob->sightRange * 2; // should not be impacted by the above

            if (plr->iSpecialState & (CN_SPECIAL_STATE_FLAG__INVISIBLE|CN_SPECIAL_STATE_FLAG__INVULNERABLE))
                mobRange = -1;

            // height is relevant for aggro distance because of platforming
            int xyDistance = hypot(mob->appearanceData.iX - plr->x, mob->appearanceData.iY - plr->y);
            int distance = hypot(xyDistance, (mob->appearanceData.iZ - plr->z) * 2); // difference in Z counts twice

            if (distance > mobRange || distance > closestDistance)
                continue;

            // found a player
            closest = s;
            closestDistance = distance;
        }
    }

    if (closest != nullptr) {
        // found closest player. engage.
        enterCombat(closest, mob);

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

    resp->iNPC_ID = mob->appearanceData.iNPC_ID;
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

        int style2 = NanoManager::nanoStyle(plr->activeNano);
        if (style2 == -1) { // no nano
            respdata[i].iHitFlag = 8;
            respdata[i].iDamage = NanoManager::SkillTable[skillID].powerIntensity[0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
        } else if (style == style2) {
            respdata[i].iHitFlag = 8; // tie
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
        } else if (style - style2 == 1 || style2 - style == 2) {
            respdata[i].iHitFlag = 4; // win
            respdata[i].iDamage = 0;
            respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina += 45;
            if (plr->Nanos[plr->activeNano].iStamina > 150)
                respdata[i].iNanoStamina = plr->Nanos[plr->activeNano].iStamina = 150;
            // fire damage power disguised as a corruption attack back at the enemy
            std::vector<int> targetData2 = {1, mob->appearanceData.iNPC_ID, 0, 0, 0};
            for (auto& pwr : NanoManager::NanoPowers)
                if (pwr.skillType == EST_DAMAGE)
                    pwr.handle(sock, targetData2, plr->activeNano, skillID, 0, 200);
        } else {
            respdata[i].iHitFlag = 16; // lose
            respdata[i].iDamage = NanoManager::SkillTable[skillID].powerIntensity[0] * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;
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
            mob->target = nullptr;
            mob->state = MobState::RETREAT;
            if (!aggroCheck(mob, getTime())) {
                clearDebuff(mob);
                if (mob->groupLeader != 0)
                    groupRetreat(mob);
            }
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
        for (auto it = mob->viewableChunks->begin(); it != mob->viewableChunks->end(); it++) {
            Chunk* chunk = *it;
            for (CNSocket *s : chunk->players) {
                Player *plr = PlayerManager::getPlayer(s);

                if (plr->HP <= 0)
                    continue;

                int distance = hypot(mob->hitX - plr->x, mob->hitY - plr->y);
                if (distance < NanoManager::SkillTable[skillID].effectArea) {
                    targetData[0] += 1;
                    targetData[targetData[0]] = plr->iID;
                    if (targetData[0] > 3) // make sure not to have more than 4
                        break;
                }
            }
        }

        for (auto& pwr : Combat::MobPowers)
            if (pwr.skillType == NanoManager::SkillTable[skillID].skillType)
                pwr.handle(mob, targetData, skillID, NanoManager::SkillTable[skillID].durationTime[0], NanoManager::SkillTable[skillID].powerIntensity[0]);
        mob->skillStyle = -3; // eruption cooldown
        mob->nextAttack = currTime + 1000;
        return;
    }

    if (mob->skillStyle == -3) { // cooldown expires
        mob->skillStyle = -1;
        return;
    }

    int random = rand() % 2000 * 1000;
    int prob1 = (int)mob->data["m_iActiveSkill1Prob"]; // active skill probability
    int prob2 = (int)mob->data["m_iCorruptionTypeProb"]; // corruption probability
    int prob3 = (int)mob->data["m_iMegaTypeProb"]; // eruption probability

    if (random < prob1) { // active skill hit
        int skillID = (int)mob->data["m_iActiveSkill1"];
        std::vector<int> targetData = {1, plr->iID, 0, 0, 0};
        for (auto& pwr : Combat::MobPowers)
            if (pwr.skillType == NanoManager::SkillTable[skillID].skillType) {
                if (pwr.bitFlag != 0 && (plr->iConditionBitFlag & pwr.bitFlag))
                    return; // prevent debuffing a player twice
                pwr.handle(mob, targetData, skillID, NanoManager::SkillTable[skillID].durationTime[0], NanoManager::SkillTable[skillID].powerIntensity[0]);
            }
        mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
        return;
    }

    if (random < prob1 + prob2) { // corruption windup
        int skillID = (int)mob->data["m_iCorruptionType"];
        INITSTRUCT(sP_FE2CL_NPC_SKILL_CORRUPTION_READY, pkt);
        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSkillID = skillID;
        pkt.iValue1 = plr->x;
        pkt.iValue2 = plr->y;
        pkt.iValue3 = plr->z;
        mob->skillStyle = NanoManager::nanoStyle(plr->activeNano) - 1;
        if (mob->skillStyle == -1)
            mob->skillStyle = 2;
        if (mob->skillStyle == -2)
            mob->skillStyle = rand() % 3;
        pkt.iStyle = mob->skillStyle;
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_SKILL_CORRUPTION_READY, sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_READY));
        mob->nextAttack = currTime + 1800;
        return;
    }

    if (random < prob1 + prob2 + prob3) { // eruption windup
        int skillID = (int)mob->data["m_iMegaType"];
        INITSTRUCT(sP_FE2CL_NPC_SKILL_READY, pkt);
        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
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

void MobAI::enterCombat(CNSocket *sock, Mob *mob) {
    mob->target = sock;
    mob->state = MobState::COMBAT;
    mob->nextMovement = getTime();
    mob->nextAttack = 0;

    mob->roamX = mob->appearanceData.iX;
    mob->roamY = mob->appearanceData.iY;
    mob->roamZ = mob->appearanceData.iZ;

    int skillID = (int)mob->data["m_iPassiveBuff"]; // cast passive
    std::vector<int> targetData = {1, mob->appearanceData.iNPC_ID, 0, 0, 0};
    for (auto& pwr : Combat::MobPowers)
        if (pwr.skillType == NanoManager::SkillTable[skillID].skillType)
            pwr.handle(mob, targetData, skillID, NanoManager::SkillTable[skillID].durationTime[0], NanoManager::SkillTable[skillID].powerIntensity[0]);

    for (NPCEvent& event : NPCManager::NPCEvents) // trigger an ON_COMBAT
        if (event.trigger == ON_COMBAT && event.npcType == mob->appearanceData.iNPCType)
            event.handler(sock, mob);
}

static void drainMobHP(Mob *mob, int amount) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_Damage);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_Damage *drain = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

    pkt->iID = mob->appearanceData.iNPC_ID;
    pkt->eCT = 4; // mob
    pkt->iTB_ID = ECSB_BOUNDINGBALL;

    drain->eCT = 4;
    drain->iID = mob->appearanceData.iNPC_ID;
    drain->iDamage = amount;
    drain->iHP = mob->appearanceData.iHP -= amount;

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);

    if (mob->appearanceData.iHP <= 0)
        Combat::killMob(mob->target, mob);
}

static void deadStep(Mob *mob, time_t currTime) {
    // despawn the mob after a short delay
    if (mob->killedTime != 0 && !mob->despawned && currTime - mob->killedTime > 2000) {
        mob->despawned = true;

        INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;

        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));

        // if it was summoned, mark it for removal
        if (mob->summoned) {
            std::cout << "[INFO] Queueing killed summoned mob for removal" << std::endl;
            RemovalQueue.push(mob->appearanceData.iNPC_ID);
            return;
        }

        // pre-set spawn coordinates if not marked for removal
        mob->appearanceData.iX = mob->spawnX;
        mob->appearanceData.iY = mob->spawnY;
        mob->appearanceData.iZ = mob->spawnZ;
    }

    // to guide their groupmates, group leaders still need to move despite being dead
    if (mob->groupLeader == mob->appearanceData.iNPC_ID)
        roamingStep(mob, currTime);

    if (mob->killedTime != 0 && currTime - mob->killedTime < mob->regenTime * 100)
        return;

    std::cout << "respawning mob " << mob->appearanceData.iNPC_ID << " with HP = " << mob->maxHealth << std::endl;

    mob->appearanceData.iHP = mob->maxHealth;
    mob->state = MobState::ROAMING;

    // if mob is a group leader/follower, spawn where the group is.
    if (mob->groupLeader != 0) {
        if (Mobs.find(mob->groupLeader) != Mobs.end()) {
            Mob* leaderMob = Mobs[mob->groupLeader];
            mob->appearanceData.iX = leaderMob->appearanceData.iX + mob->offsetX;
            mob->appearanceData.iY = leaderMob->appearanceData.iY + mob->offsetY;
            mob->appearanceData.iZ = leaderMob->appearanceData.iZ;
        } else {
            std::cout << "[WARN] deadStep: mob cannot find it's leader!" << std::endl;
        }
    }

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = mob->appearanceData;

    // notify all nearby players
    NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
}

static void combatStep(Mob *mob, time_t currTime) {
    assert(mob->target != nullptr);

    // lose aggro if the player lost connection
    if (PlayerManager::players.find(mob->target) == PlayerManager::players.end()) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        if (!aggroCheck(mob, currTime)) {
            clearDebuff(mob);
            if (mob->groupLeader != 0)
                groupRetreat(mob);
        }
        return;
    }

    Player *plr = PlayerManager::getPlayer(mob->target);

    // lose aggro if the player became invulnerable or died
    if (plr->HP <= 0
     || (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE)) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        if (!aggroCheck(mob, currTime)) {
            clearDebuff(mob);
            if (mob->groupLeader != 0)
                groupRetreat(mob);
        }
        return;
    }

    // drain
    if (mob->skillStyle < 0 && (mob->lastDrainTime == 0 || currTime - mob->lastDrainTime >= 1000)
        && mob->appearanceData.iConditionBitFlag & CSB_BIT_BOUNDINGBALL) {
        drainMobHP(mob, mob->maxHealth / 20); // lose 5% every second
        mob->lastDrainTime = currTime;
    }

    // if drain killed the mob, return early
    if (mob->appearanceData.iHP <= 0)
        return;

    // unbuffing
    std::unordered_map<int32_t, time_t>::iterator it = mob->unbuffTimes.begin();
    while (it != mob->unbuffTimes.end()) {

        if (currTime >= it->second) {
            mob->appearanceData.iConditionBitFlag &= ~it->first;

            INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
            pkt1.eCT = 2;
            pkt1.iID = mob->appearanceData.iNPC_ID;
            pkt1.iConditionBitFlag = mob->appearanceData.iConditionBitFlag;
            NPCManager::sendToViewable(mob, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
                    
            it = mob->unbuffTimes.erase(it);
        } else {
            it++;
        }
    }

    // skip attack if stunned or asleep
    if (mob->appearanceData.iConditionBitFlag & (CSB_BIT_STUN|CSB_BIT_MEZ)) {
        mob->skillStyle = -1; // in this case we also reset the any outlying abilities the mob might be winding up.
        return;
    }

    int distance = hypot(plr->x - mob->appearanceData.iX, plr->y - mob->appearanceData.iY);
    int mobRange = (int)mob->data["m_iAtkRange"] + (int)mob->data["m_iRadius"];

    if (currTime >= mob->nextAttack) {
        if (mob->skillStyle != -1 || distance <= mobRange || rand() % 20 == 0) // while not in attack range, 1 / 20 chance.
            useAbilities(mob, currTime);
        if (mob->target == nullptr)
            return;
    }

    int distanceToTravel = INT_MAX;
    int speed = mob->data["m_iRunSpeed"];
    // movement logic: move when out of range but don't move while casting a skill
    if (distance > mobRange && mob->skillStyle == -1) {
        if (mob->nextMovement != 0 && currTime < mob->nextMovement)
            return;
        mob->nextMovement = currTime + 400;
        if (currTime >= mob->nextAttack)
            mob->nextAttack = 0;

        // halve movement speed if snared
        if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
            speed /= 2;

        int targetX = plr->x;
        int targetY = plr->y;
        if (mob->groupLeader != 0) {
            targetX += mob->offsetX*distance/(mob->idleRange + 1);
            targetY += mob->offsetY*distance/(mob->idleRange + 1);
        }

        distanceToTravel = std::min(distance-mobRange+1, speed*2/5);
        auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, targetX, targetY, distanceToTravel);
        if (distanceToTravel < speed*2/5 && currTime >= mob->nextAttack)
            mob->nextAttack = 0;

        NPCManager::updateNPCPosition(mob->appearanceData.iNPC_ID, targ.first, targ.second, mob->appearanceData.iZ, mob->instanceID, mob->appearanceData.iAngle);

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = speed;
        pkt.iToX = mob->appearanceData.iX = targ.first;
        pkt.iToY = mob->appearanceData.iY = targ.second;
        pkt.iToZ = plr->z;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    /* attack logic 
     * 2/5 represents 400 ms which is the time interval mobs use per movement logic step
     * if the mob is one move interval away, we should just start attacking anyways.
     */
    if (distance <= mobRange || distanceToTravel < speed*2/5) {
        if (mob->nextAttack == 0 || currTime >= mob->nextAttack) {
            mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
            Combat::npcAttackPc(mob, currTime);
        }
    }

    // retreat if the player leaves combat range
    int xyDistance = hypot(plr->x - mob->roamX, plr->y - mob->roamY);
    distance = hypot(xyDistance, plr->z - mob->roamZ);
    if (distance >= mob->data["m_iCombatRange"]) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        clearDebuff(mob);
        if (mob->groupLeader != 0)
            groupRetreat(mob);
    }
}

void MobAI::incNextMovement(Mob *mob, time_t currTime) {
    if (currTime == 0)
        currTime = getTime();

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay/2 + rand() % (delay/2);
}

static void roamingStep(Mob *mob, time_t currTime) {
    /*
     * We reuse nextAttack to avoid scanning for players all the time, but to still
     * do so more often than if we waited for nextMovement (which is way too slow).
     * In the case of group leaders, this step will be called by dead mobs, so disable attack.
     */
    if (mob->state != MobState::DEAD && (mob->nextAttack == 0 || currTime >= mob->nextAttack)) {
        mob->nextAttack = currTime + 500;
        if (aggroCheck(mob, currTime))
            return;
    }

    // no random roaming if the mob already has a set path
    if (mob->staticPath)
        return;

    if (mob->groupLeader != 0 && mob->groupLeader != mob->appearanceData.iNPC_ID) // don't roam by yourself without group leader
        return;

    /*
     * mob->nextMovement is also updated whenever the path queue is traversed in
     * TransportManager::stepNPCPathing() (which ticks at a higher frequency than nextMovement),
     * so we don't have to check if there's already entries in the queue since we know there won't be.
     */
    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;
    incNextMovement(mob, currTime);

    int xStart = mob->spawnX - mob->idleRange/2;
    int yStart = mob->spawnY - mob->idleRange/2;
    int speed = mob->data["m_iWalkSpeed"];

    // some mobs don't move (and we mustn't divide/modulus by zero)
    if (mob->idleRange == 0 || speed == 0)
        return;

    int farX, farY, distance;
    int minDistance = mob->idleRange / 2;

    // pick a random destination
    farX = xStart + rand() % mob->idleRange;
    farY = yStart + rand() % mob->idleRange;

    distance = std::abs(std::max(farX - mob->appearanceData.iX, farY - mob->appearanceData.iY));
    if (distance == 0)
        distance += 1; // hack to avoid FPE

    // if it's too short a walk, go further in that direction
    farX = mob->appearanceData.iX + (farX - mob->appearanceData.iX) * minDistance / distance;
    farY = mob->appearanceData.iY + (farY - mob->appearanceData.iY) * minDistance / distance;

    // but don't got out of bounds
    farX = std::clamp(farX, xStart, xStart + mob->idleRange);
    farY = std::clamp(farY, yStart, yStart + mob->idleRange);

    // halve movement speed if snared
    if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
        speed /= 2;

    std::queue<WarpLocation> queue;
    WarpLocation from = { mob->appearanceData.iX, mob->appearanceData.iY, mob->appearanceData.iZ };
    WarpLocation to = { farX, farY, mob->appearanceData.iZ };

    // add a route to the queue; to be processed in TransportManager::stepNPCPathing()
    TransportManager::lerp(&queue, from, to, speed);
    TransportManager::NPCQueues[mob->appearanceData.iNPC_ID] = queue;

    if (mob->groupLeader != 0 && mob->groupLeader == mob->appearanceData.iNPC_ID) {
        // make followers follow this npc.
        for (int i = 0; i < 4; i++) {
            if (mob->groupMember[i] == 0)
                break;

            if (Mobs.find(mob->groupMember[i]) == Mobs.end()) {
                std::cout << "[WARN] roamingStep: leader can't find a group member!" << std::endl;
                continue;
            }

            std::queue<WarpLocation> queue2;
            Mob* followerMob = Mobs[mob->groupMember[i]];
            from = { followerMob->appearanceData.iX, followerMob->appearanceData.iY, followerMob->appearanceData.iZ };
            to = { farX + followerMob->offsetX, farY + followerMob->offsetY, followerMob->appearanceData.iZ };
            TransportManager::lerp(&queue2, from, to, speed);
            TransportManager::NPCQueues[followerMob->appearanceData.iNPC_ID] = queue2;
        }
    }
}

static void retreatStep(Mob *mob, time_t currTime) {
    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;

    mob->nextMovement = currTime + 400;

    // distance between spawn point and current location
    int distance = hypot(mob->appearanceData.iX - mob->roamX, mob->appearanceData.iY - mob->roamY);

    //if (distance > mob->data["m_iIdleRange"]) {
    if (distance > 10) {
        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, mob->roamX, mob->roamY, (int)mob->data["m_iRunSpeed"]*4/5);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = (int)mob->data["m_iRunSpeed"] * 2;
        pkt.iToX = mob->appearanceData.iX = targ.first;
        pkt.iToY = mob->appearanceData.iY = targ.second;
        pkt.iToZ = mob->appearanceData.iZ = mob->spawnZ;
        pkt.iMoveStyle = 1;

        // notify all nearby players
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // if we got there
    //if (distance <= mob->data["m_iIdleRange"]) {
    if (distance <= 10) { // retreat back to the spawn point
        mob->state = MobState::ROAMING;
        mob->appearanceData.iHP = mob->maxHealth;
        mob->killedTime = 0;
        mob->nextAttack = 0;
        mob->appearanceData.iConditionBitFlag = 0;

        // cast a return home heal spell, this is the right way(tm)
        std::vector<int> targetData = {1, 0, 0, 0, 0};
        for (auto& pwr : Combat::MobPowers)
            if (pwr.skillType == NanoManager::SkillTable[110].skillType)
                pwr.handle(mob, targetData, 110, NanoManager::SkillTable[110].durationTime[0], NanoManager::SkillTable[110].powerIntensity[0]);
        // clear outlying debuffs
        clearDebuff(mob);
    }
}

static void step(CNServer *serv, time_t currTime) {
    for (auto& pair : Mobs) {
        if (pair.second->playersInView < 0)
            std::cout << "[WARN] Weird playerview value " << pair.second->playersInView << std::endl;

        // skip mob movement and combat if disabled or not in view
        if ((!simulateMobs || pair.second->playersInView == 0) && pair.second->state != MobState::DEAD
        && pair.second->state != MobState::RETREAT)
            continue;

        switch (pair.second->state) {
        case MobState::INACTIVE:
            // no-op
            break;
        case MobState::ROAMING:
            roamingStep(pair.second, currTime);
            break;
        case MobState::COMBAT:
            combatStep(pair.second, currTime);
            break;
        case MobState::RETREAT:
            retreatStep(pair.second, currTime);
            break;
        case MobState::DEAD:
            deadStep(pair.second, currTime);
            break;
        }
    }

    // deallocate all NPCs queued for removal
    while (RemovalQueue.size() > 0) {
        NPCManager::destroyNPC(RemovalQueue.front());
        RemovalQueue.pop();
    }
}

void MobAI::init() {
    REGISTER_SHARD_TIMER(step, 200);

    simulateMobs = settings::SIMULATEMOBS;
}
