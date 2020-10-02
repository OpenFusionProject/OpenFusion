#include "MobManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"

#include <cmath>
#include <assert.h>

std::map<int32_t, Mob*> MobManager::Mobs;
std::queue<int32_t> MobManager::RemovalQueue;

void MobManager::init() {
    REGISTER_SHARD_TIMER(step, 200);
    REGISTER_SHARD_TIMER(playerTick, 2000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_NPCs, pcAttackNpcs);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_BEGIN, combatBegin);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_END, combatEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_DOT_DAMAGE_ONOFF, dotDamageOnOff);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_CHARs, pcAttackChars);
}

void MobManager::pcAttackNpcs(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_REQ_PC_ATTACK_NPCs* pkt = (sP_CL2FE_REQ_PC_ATTACK_NPCs*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    // sanity check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs), pkt->iNPCCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_PC_ATTACK_NPCs packet size\n";
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs));

    /*
     * Due to the possibility of multiplication overflow (and regular buffer overflow),
     * both incoming and outgoing variable-length packets must be validated, at least if
     * the number of trailing structs isn't well known (ie. it's from the client).
     */
    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC), pkt->iNPCCnt, sizeof(sAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_ATTACK_NPCs_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) + pkt->iNPCCnt * sizeof(sAttackResult);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_ATTACK_NPCs_SUCC *resp = (sP_FE2CL_PC_ATTACK_NPCs_SUCC*)respbuf;
    sAttackResult *respdata = (sAttackResult*)(respbuf+sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC));

    resp->iNPCCnt = pkt->iNPCCnt;

    for (int i = 0; i < pkt->iNPCCnt; i++) {
        if (Mobs.find(pktdata[i]) == Mobs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] pcAttackNpcs: mob ID not found" << std::endl;
            return;
        }
        Mob *mob = Mobs[pktdata[i]];

        std::pair<int,int> damage;

        if (pkt->iNPCCnt > 1)
            damage = getDamage(plr->groupDamage, (int)mob->data["m_iProtection"], true, (int)mob->data["m_iNpcLevel"]);
        else
            damage = getDamage(plr->pointDamage, (int)mob->data["m_iProtection"], true, (int)mob->data["m_iNpcLevel"]);

        damage.first = hitMob(sock, mob, damage.first);

        respdata[i].iID = mob->appearanceData.iNPC_ID;
        respdata[i].iDamage = damage.first;
        respdata[i].iHP = mob->appearanceData.iHP;
        respdata[i].iHitFlag = damage.second; // hitscan, not a rocket or a grenade
    }

    resp->iBatteryW = plr->batteryW;
    sock->sendPacket((void*)respbuf, P_FE2CL_PC_ATTACK_NPCs_SUCC, resplen);

    // a bit of a hack: these are the same size, so we can reuse the response packet
    assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_NPCs));
    sP_FE2CL_PC_ATTACK_NPCs *resp1 = (sP_FE2CL_PC_ATTACK_NPCs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_ATTACK_NPCs, resplen);
}

void MobManager::npcAttackPc(Mob *mob) {
    Player *plr = PlayerManager::getPlayer(mob->target);

    if (plr == nullptr)
        return;

    const size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) + sizeof(sAttackResult);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_ATTACK_PCs *pkt = (sP_FE2CL_NPC_ATTACK_PCs*)respbuf;
    sAttackResult *atk = (sAttackResult*)(respbuf + sizeof(sP_FE2CL_NPC_ATTACK_PCs));

    auto damage = getDamage(470 + (int)mob->data["m_iPower"], plr->defense, false, 1);
    plr->HP -= damage.first;

    pkt->iNPC_ID = mob->appearanceData.iNPC_ID;
    pkt->iPCCnt = 1;

    atk->iID = plr->iID;
    atk->iDamage = damage.first;
    atk->iHP = plr->HP;
    atk->iHitFlag = damage.second;

    mob->target->sendPacket((void*)respbuf, P_FE2CL_NPC_ATTACK_PCs, resplen);
    PlayerManager::sendToViewable(mob->target, (void*)respbuf, P_FE2CL_NPC_ATTACK_PCs, resplen);

    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
    }
}

void MobManager::giveReward(CNSocket *sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // NOTE: these will need to be scaled according to the player/mob level difference
    plr->money += (int)MissionManager::AvatarGrowth[plr->level]["m_iMobFM"]; // this one's innacurate, but close enough for now
    MissionManager::updateFusionMatter(sock, MissionManager::AvatarGrowth[plr->level]["m_iMobFM"]);

    // simple rewards
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

#if 0
    int slot = ItemManager::findFreeSlot(plr);
    if (slot == -1) {
#else
    int slot = -1;
    if (true) {
#endif
        // no room for an item, but you still get FM and taros
        reward->iItemCnt = 0;
        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, sizeof(sP_FE2CL_REP_REWARD_ITEM));
    } else {
        // item reward
        item->sItem.iType = 9;
        item->sItem.iID = 1;
        item->iSlotNum = slot;
        item->eIL = 1; // Inventory Location. 1 means player inventory.

        // update player
        plr->Inven[slot] = item->sItem;

        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    }

}

int MobManager::hitMob(CNSocket *sock, Mob *mob, int damage) {
        // cannot kill mobs multiple times; cannot harm retreating mobs
        if (mob->state != MobState::ROAMING && mob->state != MobState::COMBAT) {
            return 0; // no damage
        }

        if (mob->state == MobState::ROAMING) {
            assert(mob->target == nullptr);
            mob->target = sock;
            mob->state = MobState::COMBAT;
            mob->nextMovement = getTime();
            mob->nextAttack = 0;
        }

        mob->appearanceData.iHP -= damage;

        // wake up sleeping monster
        // TODO: remove client-side bit somehow
        mob->appearanceData.iConditionBitFlag &= ~CSB_BIT_MEZ;

        if (mob->appearanceData.iHP <= 0)
            killMob(mob->target, mob);

        return damage;
}

void MobManager::killMob(CNSocket *sock, Mob *mob) {
    mob->state = MobState::DEAD;
    mob->target = nullptr;
    mob->appearanceData.iConditionBitFlag = 0;
    mob->killedTime = getTime(); // XXX: maybe introduce a shard-global time for each step?

    // check for the edge case where hitting the mob did not aggro it
    if (sock != nullptr) {
        giveReward(sock);
        MissionManager::mobKilled(sock, mob->appearanceData.iNPCType);
    }

    mob->despawned = false;
}

void MobManager::deadStep(Mob *mob, time_t currTime) {
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
    }

    if (mob->killedTime != 0 && currTime - mob->killedTime < mob->regenTime * 100)
        return;

    std::cout << "respawning mob " << mob->appearanceData.iNPC_ID << " with HP = " << mob->maxHealth << std::endl;

    mob->appearanceData.iHP = mob->maxHealth;
    mob->state = MobState::ROAMING;

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = mob->appearanceData;

    // notify all nearby players
    NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
}

void MobManager::combatStep(Mob *mob, time_t currTime) {
    assert(mob->target != nullptr);

    // sanity check: did the target player lose connection?
    if (PlayerManager::players.find(mob->target) == PlayerManager::players.end()) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        return;
    }
    Player *plr = PlayerManager::getPlayer(mob->target);

    if (plr == nullptr)
        return;

    // did something else kill the player in the mean time?
    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        return;
    }

    int distance = hypot(plr->x - mob->appearanceData.iX, plr->y - mob->appearanceData.iY);

    /*
     * If the mob is close enough to attack, do so. If not, get closer.
     * No, I'm not 100% sure this is how it's supposed to work.
     */
    if (distance <= (int)mob->data["m_iAtkRange"]) {
        // attack logic
        if (mob->nextAttack == 0) {
            mob->nextAttack = currTime + (int)mob->data["m_iInitalTime"] * 100; // I *think* this is what this is
            npcAttackPc(mob);
        } else if (mob->nextAttack != 0 && currTime >= mob->nextAttack) {
            mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
            npcAttackPc(mob);
        }
    } else {
        // movement logic
        if (mob->nextMovement != 0 && currTime < mob->nextMovement)
            return;
        mob->nextMovement = currTime + 500;

        int speed = mob->data["m_iRunSpeed"];

        // halve movement speed if snared
        if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
            speed /= 2;

        auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, mob->target->plr->x, mob->target->plr->y, speed);

        NPCManager::updateNPCPosition(mob->appearanceData.iNPC_ID, targ.first, targ.second, mob->appearanceData.iZ);

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = speed;
        pkt.iToX = mob->appearanceData.iX = targ.first;
        pkt.iToY = mob->appearanceData.iY = targ.second;
        pkt.iToZ = mob->appearanceData.iZ;

        // notify all nearby players
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // retreat if the player leaves combat range
    distance = hypot(plr->x - mob->spawnX, plr->y - mob->spawnY);
    if (distance >= mob->data["m_iCombatRange"]) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
    }
}

/*
 * TODO: precalculate a path, lerp through it, repeat.
 * The way it works right now, mobs only move around a little bit. This will result
 * in more natural movement, and will mesh well with pre-calculated paths (for Don Doom,
 * Bad Max, etc.) once those have been made.
 */
void MobManager::roamingStep(Mob *mob, time_t currTime) {
    /*
     * We reuse nextAttack to avoid scanning for players all the time, but to still
     * do so more often than if we waited for nextMovement (which is way too slow).
     */
    if (mob->nextAttack == 0 || currTime >= mob->nextAttack) {
        mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;

        /*
         * Aggro on nearby players.
         * Even if they're in range, we can't assume they're all in the same one chunk
         * as the mob, since it might be near a chunk boundary.
         */
        for (Chunk *chunk : mob->currentChunks) {
            for (CNSocket *s : chunk->players) {
                Player *plr = s->plr;

                // height is relevant for aggro distance because of platforming
                int xyDistance = hypot(mob->appearanceData.iX - plr->x, mob->appearanceData.iY - plr->y);
                int distance = hypot(xyDistance, mob->appearanceData.iZ - plr->z);
                if (distance > mob->data["m_iSightRange"])
                    continue;

                // found player. engage.
                mob->target = s;
                mob->state = MobState::COMBAT;
                mob->nextMovement = currTime;
                mob->nextAttack = 0;
                return;
            }
        }
    }

    // some mobs don't move (and we mustn't divide/modulus by zero)
    if (mob->idleRange == 0)
        return;

    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay/2 + rand() % (delay/2);

    INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);
    int xStart = mob->spawnX - mob->idleRange/2;
    int yStart = mob->spawnY - mob->idleRange/2;

    int farX = xStart + rand() % mob->idleRange;
    int farY = yStart + rand() % mob->idleRange;
    int speed = mob->data["m_iWalkSpeed"];

    // halve movement speed if snared
    if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
        speed /= 2;

    auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, farX, farY, speed);

    NPCManager::updateNPCPosition(mob->appearanceData.iNPC_ID, targ.first, targ.second, mob->appearanceData.iZ);

    pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
    pkt.iSpeed = speed;
    pkt.iToX = mob->appearanceData.iX = targ.first;
    pkt.iToY = mob->appearanceData.iY = targ.second;
    pkt.iToZ = mob->appearanceData.iZ;

    // notify all nearby players
    NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
}

void MobManager::retreatStep(Mob *mob, time_t currTime) {
    // distance between spawn point and current location
    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;

    mob->nextMovement = currTime + 500;
    
    int distance = hypot(mob->appearanceData.iX - mob->spawnX, mob->appearanceData.iY - mob->spawnY);

    if (distance > mob->data["m_iIdleRange"]) {
        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, mob->spawnX, mob->spawnY, mob->data["m_iRunSpeed"]);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = mob->data["m_iRunSpeed"];
        pkt.iToX = mob->appearanceData.iX = targ.first;
        pkt.iToY = mob->appearanceData.iY = targ.second;
        pkt.iToZ = mob->appearanceData.iZ;

        // notify all nearby players
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // if we got there
    if (distance <= mob->data["m_iIdleRange"]) {
        mob->state = MobState::ROAMING;
        mob->appearanceData.iHP = mob->maxHealth;
        mob->killedTime = 0;
        mob->nextAttack = 0;
        mob->appearanceData.iConditionBitFlag = 0;
        
        //INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
        //enterData.NPCAppearanceData = mob->appearanceData;
        //NPCManager::sendToViewable(mob, &enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
        resendMobHP(mob);
    }
}

void MobManager::step(CNServer *serv, time_t currTime) {

    for (auto& pair : Mobs) {
        int x = pair.second->appearanceData.iX;
        int y = pair.second->appearanceData.iY;

        // skip chunks without players
        if (!ChunkManager::inPopulatedChunks(x, y, pair.second->instanceID))
            continue;

        // skip mob movement and combat if disabled
        if (!settings::SIMULATEMOBS && pair.second->state != MobState::DEAD)
            continue;

        // skip attack/move if stunned or asleep
        if (pair.second->appearanceData.iConditionBitFlag & (CSB_BIT_STUN|CSB_BIT_MEZ)
        && (pair.second->state == MobState::ROAMING || pair.second->state == MobState::COMBAT))
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

/*
 * Dynamic lerp; distinct from TransportManager::lerp(). This one doesn't care about height and
 * only returns the first step, since the rest will need to be recalculated anyway if chasing player.
 */
std::pair<int,int> MobManager::lerp(int x1, int y1, int x2, int y2, int speed) {
    std::pair<int,int> ret = {x1, y1};

    speed /= 2;
    
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

void MobManager::combatBegin(CNSocket *sock, CNPacketData *data) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr != nullptr)
        plr->inCombat = true;
}

void MobManager::combatEnd(CNSocket *sock, CNPacketData *data) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr != nullptr)
        plr->inCombat = false;
}

void MobManager::dotDamageOnOff(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_DOT_DAMAGE_ONOFF *pkt = (sP_CL2FE_DOT_DAMAGE_ONOFF*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr != nullptr)
        plr->dotDamage = (bool)pkt->iFlag;
}

void MobManager::dealGooDamage(CNSocket *sock, int amount) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_DotDamage);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_DotDamage *dmg = (sSkillResult_DotDamage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    // update player
    plr->HP -= amount;

    pkt->iID = plr->iID;
    pkt->eCT = 1; // player
    pkt->iTB_ID = ECSB_INFECTION; // sSkillResult_DotDamage

    dmg->eCT = 1;
    dmg->iID = plr->iID;
    dmg->iDamage = amount;
    dmg->iHP = plr->HP;

    sock->sendPacket((void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
}

void MobManager::playerTick(CNServer *serv, time_t currTime) {
    static time_t lastHealTime = 0;

    for (auto& pair : PlayerManager::players) {
        CNSocket *sock = pair.first;
        Player *plr = pair.second.plr;
        bool transmit = false;

        // do not tick dead players
        if (plr->HP <= 0)
            continue;

        // fm patch/lake damage
        if (plr->dotDamage)
            dealGooDamage(sock, PC_MAXHEALTH(plr->level) * 3 / 20);

        // heal
        if (currTime - lastHealTime >= 6000 && !plr->inCombat && plr->HP < PC_MAXHEALTH(plr->level)) {
            plr->HP += PC_MAXHEALTH(plr->level) / 5;
            if (plr->HP > PC_MAXHEALTH(plr->level))
                plr->HP = PC_MAXHEALTH(plr->level);
            transmit = true;
        }

        for (int i = 0; i < 3; i++) {
            if (plr->activeNano != 0 && plr->equippedNanos[i] == plr->activeNano) { // spend stamina
                plr->Nanos[plr->activeNano].iStamina -= 1;
                
                if (plr->passiveNanoOut)
                   plr->Nanos[plr->activeNano].iStamina -= 1; 

                if (plr->Nanos[plr->activeNano].iStamina < 0)
                    plr->activeNano = 0;

                transmit = true;
            } else if (plr->Nanos[plr->equippedNanos[i]].iStamina < 150) { // regain stamina
                sNano& nano = plr->Nanos[plr->equippedNanos[i]];
                nano.iStamina += 1;

                if (nano.iStamina > 150)
                    nano.iStamina = 150;

                transmit = true;
            }
        }

        if (transmit) {
            INITSTRUCT(sP_FE2CL_REP_PC_TICK, pkt);

            pkt.iHP = plr->HP;
            pkt.iBatteryN = plr->batteryN;

            pkt.aNano[0] = plr->Nanos[plr->equippedNanos[0]];
            pkt.aNano[1] = plr->Nanos[plr->equippedNanos[1]];
            pkt.aNano[2] = plr->Nanos[plr->equippedNanos[2]];

            sock->sendPacket((void*)&pkt, P_FE2CL_REP_PC_TICK, sizeof(sP_FE2CL_REP_PC_TICK));
        }
    }

    // if this was a heal tick, update the counter outside of the loop
    if (currTime - lastHealTime >= 6000)
        lastHealTime = currTime;
}

std::pair<int,int> MobManager::getDamage(int attackPower, int defensePower, bool shouldCrit, int crutchLevel) {

    std::pair<int,int> ret = {};

    int damage = (std::max(40, attackPower - defensePower) * (34 + crutchLevel) + std::min(attackPower, attackPower * attackPower / defensePower) * (36 - crutchLevel)) / 70;
    ret.first = damage * (rand() % 40 + 80) / 100; // 20% variance
    ret.second = 1;

    if (shouldCrit && rand() % 20 == 0) {
        ret.first *= 2; // critical hit
        ret.second = 2;
    }

    return ret;
}

void MobManager::pcAttackChars(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_REQ_PC_ATTACK_CHARs* pkt = (sP_CL2FE_REQ_PC_ATTACK_CHARs*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    // Unlike the attack mob packet, attacking players packet has an 8-byte trail (Instead of 4 bytes).
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs), pkt->iTargetCnt, sizeof(int32_t) * 2, data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_PC_ATTACK_CHARs packet size\n";
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs));

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC), pkt->iTargetCnt, sizeof(sAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_ATTACK_CHARs_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) + pkt->iTargetCnt * sizeof(sAttackResult);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_ATTACK_CHARs_SUCC *resp = (sP_FE2CL_PC_ATTACK_CHARs_SUCC*)respbuf;
    sAttackResult *respdata = (sAttackResult*)(respbuf+sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC));

    resp->iTargetCnt = pkt->iTargetCnt;

    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (pktdata[i*2+1] == 1) { // eCT == 1; attack player
            Player *target = nullptr;

            for (auto& pair : PlayerManager::players) {
                if (pair.second.plr->iID == pktdata[i*2]) {
                    target = pair.second.plr;
                    break;
                }
            }

            if (target == nullptr) {
                // you shall not pass
                std::cout << "[WARN] pcAttackChars: player ID not found" << std::endl;
                return;
            }
            
            std::pair<int,int> damage;
            
            if (pkt->iTargetCnt > 1)
                damage = getDamage(plr->groupDamage, target->defense, true, 1);
            else
                damage = getDamage(plr->pointDamage, target->defense, true, 1);

            target->HP -= damage.first;

            respdata[i].eCT = pktdata[i*2+1];
            respdata[i].iID = target->iID;
            respdata[i].iDamage = damage.first;
            respdata[i].iHP = target->HP;
            respdata[i].iHitFlag = damage.second; // hitscan, not a rocket or a grenade
        } else { // eCT == 4; attack mob
            if (Mobs.find(pktdata[i*2]) == Mobs.end()) {
                // not sure how to best handle this
                std::cout << "[WARN] pcAttackNpcs: mob ID not found" << std::endl;
                return;
            }
            Mob *mob = Mobs[pktdata[i*2]];

            std::pair<int,int> damage;

            if (pkt->iTargetCnt > 1)
                damage = getDamage(plr->groupDamage, (int)mob->data["m_iProtection"], true, (int)mob->data["m_iNpcLevel"]);
            else
                damage = getDamage(plr->pointDamage, (int)mob->data["m_iProtection"], true, (int)mob->data["m_iNpcLevel"]);

            damage.first = hitMob(sock, mob, damage.first);

            respdata[i].eCT = pktdata[i*2+1];
            respdata[i].iID = mob->appearanceData.iNPC_ID;
            respdata[i].iDamage = damage.first;
            respdata[i].iHP = mob->appearanceData.iHP;
            respdata[i].iHitFlag = damage.second; // hitscan, not a rocket or a grenade
        }
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_PC_ATTACK_CHARs_SUCC, resplen);

    // a bit of a hack: these are the same size, so we can reuse the response packet
    assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_CHARs));
    sP_FE2CL_PC_ATTACK_CHARs *resp1 = (sP_FE2CL_PC_ATTACK_CHARs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_ATTACK_CHARs, resplen);
}

void MobManager::resendMobHP(Mob *mob) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_Heal_HP);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_Heal_HP *heal = (sSkillResult_Heal_HP*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

    pkt->iID = mob->appearanceData.iNPC_ID;
    pkt->eCT = 4; // mob
    pkt->iTB_ID = ECSB_HEAL; // sSkillResult_Heal_HP

    heal->eCT = 4;
    heal->iID = mob->appearanceData.iNPC_ID;
    heal->iHealHP = 0;
    heal->iHP = mob->appearanceData.iHP;

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
}
