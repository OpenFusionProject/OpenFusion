#include "MobManager.hpp"
#include "PlayerManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"
#include "GroupManager.hpp"
#include "TransportManager.hpp"

#include <cmath>
#include <limits.h>
#include <assert.h>

std::map<int32_t, Mob*> MobManager::Mobs;
std::queue<int32_t> MobManager::RemovalQueue;

std::map<int32_t, MobDropChance> MobManager::MobDropChances;
std::map<int32_t, MobDrop> MobManager::MobDrops;

bool MobManager::simulateMobs;

void MobManager::init() {
    REGISTER_SHARD_TIMER(step, 200);
    REGISTER_SHARD_TIMER(playerTick, 2000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_NPCs, pcAttackNpcs);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_BEGIN, combatBegin);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_END, combatEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_DOT_DAMAGE_ONOFF, dotDamageOnOff);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_CHARs, pcAttackChars);

    simulateMobs = settings::SIMULATEMOBS;
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
            damage.first = plr->groupDamage;
        else
            damage.first = plr->pointDamage;

        int difficulty = (int)mob->data["m_iNpcLevel"];
        damage = getDamage(damage.first, (int)mob->data["m_iProtection"], true, (plr->batteryW > 6 + difficulty), NanoManager::nanoStyle(plr->activeNano), (int)mob->data["m_iNpcStyle"], difficulty);
        
        if (plr->batteryW >= 6 + difficulty)
            plr->batteryW -= 6 + difficulty;
        else
            plr->batteryW = 0;

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

void MobManager::npcAttackPc(Mob *mob, time_t currTime) {
    Player *plr = PlayerManager::getPlayer(mob->target);

    if (plr == nullptr)
        return;

    const size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) + sizeof(sAttackResult);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_ATTACK_PCs *pkt = (sP_FE2CL_NPC_ATTACK_PCs*)respbuf;
    sAttackResult *atk = (sAttackResult*)(respbuf + sizeof(sP_FE2CL_NPC_ATTACK_PCs));

    auto damage = getDamage(450 + (int)mob->data["m_iPower"], plr->defense, false, false, -1, -1, rand() % plr->level + 1);
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
        aggroCheck(mob, currTime);
    }
}

void MobManager::giveReward(CNSocket *sock, Mob* mob) {
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

    // sanity check
    if (MobDrops.find(mob->dropType) == MobDrops.end()) {
        std::cout << "[WARN] Drop Type " << mob->dropType << " was not found" << std::endl;
        return;
    }
    // find correct mob drop
    MobDrop& drop = MobDrops[mob->dropType];

    plr->money += drop.taros;
    // formula for scaling FM with player/mob level difference
    // TODO: adjust this better
    int levelDifference = plr->level - mob->level;
    int fm = drop.fm;
    if (levelDifference > 0)
        fm = levelDifference < 10 ? fm - (levelDifference * fm / 10) : 0;

    MissionManager::updateFusionMatter(sock, fm);

    // give boosts 1 in 3 times
    if (drop.boosts > 0) {
        if (rand() % 3 == 0)
            plr->batteryN += drop.boosts;
        if (rand() % 3 == 0)
            plr->batteryW += drop.boosts;
    }

    // simple rewards
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->m_iBatteryN = plr->batteryN;
    reward->m_iBatteryW = plr->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    int slot = ItemManager::findFreeSlot(plr);

    bool awardDrop = false;
    MobDropChance *chance = nullptr;
    // sanity check
    if (MobDropChances.find(drop.dropChanceType) == MobDropChances.end()) {
        std::cout << "[WARN] Unknown Drop Chance Type: " << drop.dropChanceType << std::endl;
        return; // this also prevents holiday crate drops, but oh well
    } else {
        chance = &MobDropChances[drop.dropChanceType];
        awardDrop = (rand() % 1000 < chance->dropChance);
    }

    // no drop
    if (slot == -1 || !awardDrop) {
        // no room for an item, but you still get FM and taros
        reward->iItemCnt = 0;
        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, sizeof(sP_FE2CL_REP_REWARD_ITEM));
    } else {
        // item reward
        getReward(&item->sItem, &drop, chance);
        item->iSlotNum = slot;
        item->eIL = 1; // Inventory Location. 1 means player inventory.

        // update player
        plr->Inven[slot] = item->sItem;

        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    }

    // event crates
    if (settings::EVENTMODE != 0)
        giveEventReward(sock, plr);
}

void MobManager::getReward(sItemBase *reward, MobDrop* drop, MobDropChance* chance) {
    reward->iType = 9;
    reward->iOpt = 1;

    int total = 0;
    for (int ratio : chance->cratesRatio)
        total += ratio;

    // randomizing a crate
    int randomNum = rand() % total;
    int i = 0;
    int sum = 0;
    do {
        reward->iID = drop->crateIDs[i];
        sum += chance->cratesRatio[i];
        i++;
    }
    while (sum<=randomNum);
}

void MobManager::giveEventReward(CNSocket* sock, Player* player) {
    // random drop chance
    if (rand() % 100 > settings::EVENTCRATECHANCE)
        return;

    // no slot = no reward
    int slot = ItemManager::findFreeSlot(player);
    if (slot == -1)
        return;

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);

    uint8_t respbuf[resplen];
    sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
    sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // leave everything here as it is
    reward->m_iCandy = player->money;
    reward->m_iFusionMatter = player->fusionmatter;
    reward->m_iBatteryN = player->batteryN;
    reward->m_iBatteryW = player->batteryW;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    // which crate to drop
    int crateId;
    switch (settings::EVENTMODE) {
    // knishmas
    case 1: crateId = 1187; break;
    // halloween
    case 2: crateId = 1181; break;
    // spring
    case 3: crateId = 1126; break;
    // what
    default:
        std::cout << "[WARN] Unknown event Id " << settings::EVENTMODE << std::endl;
        return;
    }

    item->sItem.iType = 9;
    item->sItem.iID = crateId;
    item->sItem.iOpt = 1;
    item->iSlotNum = slot;
    item->eIL = 1; // Inventory Location. 1 means player inventory.

    // update player
    player->Inven[slot] = item->sItem;
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
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

        mob->roamX = mob->appearanceData.iX;
        mob->roamY = mob->appearanceData.iY;
        mob->roamZ = mob->appearanceData.iZ;
    }

    mob->appearanceData.iHP -= damage;

    // wake up sleeping monster
    if (mob->appearanceData.iConditionBitFlag & CSB_BIT_MEZ) {
        mob->appearanceData.iConditionBitFlag &= ~CSB_BIT_MEZ;

        INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
        pkt1.eCT = 2;
        pkt1.iID = mob->appearanceData.iNPC_ID;
        pkt1.iConditionBitFlag = mob->appearanceData.iConditionBitFlag;
        NPCManager::sendToViewable(mob, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
    }

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
        Player* plr = PlayerManager::getPlayer(sock);

        if (plr == nullptr)
            return;

        if (plr->groupCnt == 1 && plr->iIDGroup == plr->iID) {
            giveReward(sock, mob);
            MissionManager::mobKilled(sock, mob->appearanceData.iNPCType);
        } else {
            plr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            if (plr == nullptr)
                return;

            for (int i = 0; i < plr->groupCnt; i++) {
                CNSocket* sockTo = PlayerManager::getSockFromID(plr->groupIDs[i]);
                giveReward(sockTo, mob);
                MissionManager::mobKilled(sockTo, mob->appearanceData.iNPCType);
            }
        }
    }

    // delay the despawn animation
    mob->despawned = false;

    auto it = TransportManager::NPCQueues.find(mob->appearanceData.iNPC_ID);
    if (it == TransportManager::NPCQueues.end() || it->second.empty())
        return;

    // rewind or empty the movement queue
    if (mob->staticPath) {
        /*
         * This is inelegant, but we wind forward in the path until we find the point that
         * corresponds with the Mob's spawn point.
         *
         * IMPORTANT: The check in TableData::loadPaths() must pass or else this will loop forever.
         */
        auto& queue = it->second;
        for (auto point = queue.front(); point.x != mob->spawnX || point.y != mob->spawnY; point = queue.front()) {
            queue.pop();
            queue.push(point);
        }
    } else {
        TransportManager::NPCQueues.erase(mob->appearanceData.iNPC_ID);
    }
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

    // reset position
    mob->appearanceData.iX = mob->spawnX;
    mob->appearanceData.iY = mob->spawnY;
    mob->appearanceData.iZ = mob->spawnZ;

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
        aggroCheck(mob, currTime);
        return;
    }

    Player *plr = PlayerManager::getPlayer(mob->target);

    if (plr == nullptr)
        return;

    // did something else kill the player in the mean time?
    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        aggroCheck(mob, currTime);
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
            INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);
            pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
            pkt.iSpeed = (int)mob->data["m_iRunSpeed"];
            pkt.iToX = mob->appearanceData.iX;
            pkt.iToY = mob->appearanceData.iY;
            pkt.iToZ = mob->target->plr->z;
            NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));

            mob->nextAttack = currTime + (int)mob->data["m_iInitalTime"] * 100; //I *think* this is what this is
            npcAttackPc(mob, currTime);
        } else if (mob->nextAttack != 0 && currTime >= mob->nextAttack) {
            mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
            npcAttackPc(mob, currTime);
        }
    } else {
        // movement logic
        if (mob->nextMovement != 0 && currTime < mob->nextMovement)
            return;
        mob->nextMovement = currTime + 400;
        if (currTime >= mob->nextAttack)
            mob->nextAttack = 0;

        int speed = mob->data["m_iRunSpeed"];

        // halve movement speed if snared
        if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
            speed /= 2;

        auto targ = lerp(mob->appearanceData.iX, mob->appearanceData.iY, mob->target->plr->x, mob->target->plr->y,std::min(distance-(int)mob->data["m_iAtkRange"]+1, speed*2/5));

        NPCManager::updateNPCPosition(mob->appearanceData.iNPC_ID, targ.first, targ.second, mob->appearanceData.iZ);

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = speed;
        pkt.iToX = mob->appearanceData.iX = targ.first;
        pkt.iToY = mob->appearanceData.iY = targ.second;
        pkt.iToZ = mob->target->plr->z;

        // notify all nearby players
        NPCManager::sendToViewable(mob, &pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
    }

    // retreat if the player leaves combat range
    int xyDistance = hypot(plr->x - mob->roamX, plr->y - mob->roamY);
    distance = hypot(xyDistance, plr->z - mob->roamZ);
    if (distance >= mob->data["m_iCombatRange"]) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
    }
}

void MobManager::incNextMovement(Mob *mob, time_t currTime) {
    if (currTime == 0)
        currTime = getTime();

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay/2 + rand() % (delay/2);
}

void MobManager::roamingStep(Mob *mob, time_t currTime) {
    /*
     * We reuse nextAttack to avoid scanning for players all the time, but to still
     * do so more often than if we waited for nextMovement (which is way too slow).
     */
    if (mob->nextAttack == 0 || currTime >= mob->nextAttack) {
        mob->nextAttack = currTime + 500;
        if (aggroCheck(mob, currTime))
            return;
    }

    // some mobs don't move (and we mustn't divide/modulus by zero)
    if (mob->idleRange == 0)
        return;

    // no random roaming if the mob already has a set path
    if (mob->staticPath)
        return;

    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;
    incNextMovement(mob, currTime);
    /*
     * mob->nextMovement is also updated whenever the path queue is traversed in
     * TransportManager::stepNPCPathing() (which ticks at a higher frequency than nextMovement),
     * so we don't have to check if there's already entries in the queue since we know there won't be.
     */

    int xStart = mob->spawnX - mob->idleRange/2;
    int yStart = mob->spawnY - mob->idleRange/2;
    int speed = mob->data["m_iWalkSpeed"];

    int farX, farY;
    int distance; // for short walk detection

    /*
     * We don't want the mob to just take one step and stop, so we make sure
     * it has walked a half-decent distance.
     */
    do {
        farX = xStart + rand() % mob->idleRange;
        farY = yStart + rand() % mob->idleRange;

        distance = std::hypot(mob->appearanceData.iX - farX, mob->appearanceData.iY - farY);
    } while (distance < 500);

    // halve movement speed if snared
    if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED)
        speed /= 2;

    std::queue<WarpLocation> queue;
    WarpLocation from = { mob->appearanceData.iX, mob->appearanceData.iY, mob->appearanceData.iZ };
    WarpLocation to = { farX, farY, mob->appearanceData.iZ };

    // add a route to the queue; to be processed in TransportManager::stepNPCPathing()
    TransportManager::lerp(&queue, from, to, speed);
    TransportManager::NPCQueues[mob->appearanceData.iNPC_ID] = queue;
}

void MobManager::retreatStep(Mob *mob, time_t currTime) {
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
        
        // HACK: we haven't found a better way to refresh a mob's client-side status
        drainMobHP(mob, 0);
    }
}

void MobManager::step(CNServer *serv, time_t currTime) {
    static time_t lastDrainTime = 0;
    
    for (auto& pair : Mobs) {
        int x = pair.second->appearanceData.iX;
        int y = pair.second->appearanceData.iY;

        // skip chunks without players
        if (!ChunkManager::inPopulatedChunks(x, y, pair.second->instanceID))
            continue;

        // drain
        if (currTime - lastDrainTime >= 600 && pair.second->appearanceData.iConditionBitFlag & CSB_BIT_BOUNDINGBALL) {
            drainMobHP(pair.second, pair.second->maxHealth * 3 / 50); // lose 10% every second
        }

        // unbuffing
        std::unordered_map<int32_t, time_t>::iterator it = pair.second->unbuffTimes.begin();
        while (it != pair.second->unbuffTimes.end()) {
            
            if (currTime >= it->second) {
                pair.second->appearanceData.iConditionBitFlag &= ~it->first;
                
                INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, pkt1);
                pkt1.eCT = 2;
                pkt1.iID = pair.second->appearanceData.iNPC_ID;
                pkt1.iConditionBitFlag = pair.second->appearanceData.iConditionBitFlag;
                NPCManager::sendToViewable(pair.second, &pkt1, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
                
                it = pair.second->unbuffTimes.erase(it);
            } else {
                it++;
            }
        }

        // skip mob movement and combat if disabled
        if (!simulateMobs && pair.second->state != MobState::DEAD
        && pair.second->state != MobState::RETREAT)
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
    
    if (currTime - lastDrainTime >= 600)
        lastDrainTime = currTime;

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

    if (plr == nullptr) {
        std::cout << "[WARN] combatBegin: null player!" << std::endl;
        return;
    }

    plr->inCombat = true;

    // HACK: make sure the player has the right weapon out for combat
    INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, resp);

    resp.iPC_ID = plr->iID;
    resp.iEquipSlotNum = 0;
    resp.EquipSlotItem = plr->Equip[0];

    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
}

void MobManager::combatEnd(CNSocket *sock, CNPacketData *data) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr != nullptr) {
        plr->inCombat = false;
        plr->healCooldown = 4000;
    }
}

void MobManager::dotDamageOnOff(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_DOT_DAMAGE_ONOFF *pkt = (sP_CL2FE_DOT_DAMAGE_ONOFF*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr) {
        std::cout << "[WARN] dotDamageOnOff: null player!" << std::endl;
        return;
    }

    if ((plr->iConditionBitFlag & CSB_BIT_INFECTION) != (bool)pkt->iFlag)
        plr->iConditionBitFlag ^= CSB_BIT_INFECTION;

    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt1);

    pkt1.eCSTB = ECSB_INFECTION; // eCharStatusTimeBuffID
    pkt1.eTBU = 1; // eTimeBuffUpdate
    pkt1.eTBT = 0; // eTimeBuffType 1 means nano
    pkt1.iConditionBitFlag = plr->iConditionBitFlag;

    sock->sendPacket((void*)&pkt1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

void MobManager::dealGooDamage(CNSocket *sock, int amount) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_DotDamage);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_DotDamage *dmg = (sSkillResult_DotDamage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

    if (plr->iConditionBitFlag & CSB_BIT_PROTECT_INFECTION) {
        amount = -2; // -2 is the magic number for "Protected" to appear as the damage number
        dmg->bProtected = 1;

        // it's hypothetically possible to have the protection bit without a nano
        if (plr->activeNano != -1)
            plr->Nanos[plr->activeNano].iStamina -= 3;
    } else {
        plr->HP -= amount;
    }

    if (plr->activeNano != -1) {
        dmg->iStamina = plr->Nanos[plr->activeNano].iStamina;

        if (plr->Nanos[plr->activeNano].iStamina <= 0) {
            dmg->bNanoDeactive = 1;
            plr->Nanos[plr->activeNano].iStamina = 0;
            NanoManager::summonNano(PlayerManager::getSockFromID(plr->iID), -1);
        }
    }

    pkt->iID = plr->iID;
    pkt->eCT = 1; // player
    pkt->iTB_ID = ECSB_INFECTION; // sSkillResult_DotDamage

    dmg->eCT = 1;
    dmg->iID = plr->iID;
    dmg->iDamage = amount;
    dmg->iHP = plr->HP;
    dmg->iConditionBitFlag = plr->iConditionBitFlag;

    sock->sendPacket((void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
}

void MobManager::playerTick(CNServer *serv, time_t currTime) {
    static time_t lastHealTime = 0;

    for (auto& pair : PlayerManager::players) {
        CNSocket *sock = pair.first;
        Player *plr = pair.second.plr;
        bool transmit = false;

        // group ticks
        if (plr->groupCnt > 1)
            GroupManager::groupTickInfo(plr);

        // do not tick dead players
        if (plr->HP <= 0)
            continue;

        // fm patch/lake damage
        if (plr->iConditionBitFlag & CSB_BIT_INFECTION)
            dealGooDamage(sock, PC_MAXHEALTH(plr->level) * 3 / 20);

        // heal
        if (currTime - lastHealTime >= 4000 && !plr->inCombat && plr->HP < PC_MAXHEALTH(plr->level)) {
            if (currTime - lastHealTime - plr->healCooldown >= 4000) {
                plr->HP += PC_MAXHEALTH(plr->level) / 5;
                if (plr->HP > PC_MAXHEALTH(plr->level))
                    plr->HP = PC_MAXHEALTH(plr->level);
                transmit = true;
            } else
                plr->healCooldown -= 4000;
        }

        for (int i = 0; i < 3; i++) {
            if (plr->activeNano != 0 && plr->equippedNanos[i] == plr->activeNano) { // spend stamina
                plr->Nanos[plr->activeNano].iStamina -= 1;

                if (plr->passiveNanoOut)
                   plr->Nanos[plr->activeNano].iStamina -= 1;

                if (plr->Nanos[plr->activeNano].iStamina <= 0) {
                    plr->Nanos[plr->activeNano].iStamina = 0;
                    NanoManager::summonNano(PlayerManager::getSockFromID(plr->iID), -1);
                }

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
    if (currTime - lastHealTime >= 4000)
        lastHealTime = currTime;
}

std::pair<int,int> MobManager::getDamage(int attackPower, int defensePower, bool shouldCrit,
                                         bool batteryBoost, int attackerStyle,
                                         int defenderStyle, int difficulty) {
    std::pair<int,int> ret = {0, 1};
    if (attackPower + defensePower * 2 == 0)
        return ret;

    // base calculation
    int damage = attackPower * attackPower / (attackPower + defensePower);
    damage = std::max(10 + attackPower / 10, damage - defensePower * (4 + difficulty) / 40);
    damage = damage * (rand() % 40 + 80) / 100;

    // Adaptium/Blastons/Cosmix
    if (attackerStyle != -1 && defenderStyle != -1 && attackerStyle != defenderStyle) {
        if (attackerStyle < defenderStyle || attackerStyle - defenderStyle == 2) 
            damage = damage * 3 / 2;
        else
            damage = damage * 2 / 3;
    }

    // weapon boosts
    if (batteryBoost)
        damage = damage * 5 / 4;

    ret.first = damage;
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
                damage.first = plr->groupDamage;
            else
                damage.first = plr->pointDamage;

            damage = getDamage(damage.first, target->defense, true, (plr->batteryW > 6 + plr->level), -1, -1, 1);

            if (plr->batteryW >= 6 + plr->level)
                plr->batteryW -= 6 + plr->level;
            else
                plr->batteryW = 0;

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
                damage.first = plr->groupDamage;
            else
                damage.first = plr->pointDamage;

            int difficulty = (int)mob->data["m_iNpcLevel"];

            damage = getDamage(damage.first, (int)mob->data["m_iProtection"], true, (plr->batteryW > 6 + difficulty),
                NanoManager::nanoStyle(plr->activeNano), (int)mob->data["m_iNpcStyle"], difficulty);

            if (plr->batteryW >= 6 + difficulty)
                plr->batteryW -= 6 + difficulty;
            else
                plr->batteryW = 0;

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
    assert(sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_CHARs));
    sP_FE2CL_PC_ATTACK_CHARs *resp1 = (sP_FE2CL_PC_ATTACK_CHARs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_ATTACK_CHARs, resplen);
}

void MobManager::drainMobHP(Mob *mob, int amount) {
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
}

/*
 * Aggro on nearby players.
 * Even if they're in range, we can't assume they're all in the same one chunk
 * as the mob, since it might be near a chunk boundary.
 */
bool MobManager::aggroCheck(Mob *mob, time_t currTime) {
    CNSocket *closest = nullptr;
    int closestDistance = INT_MAX;

    for (Chunk *chunk : mob->currentChunks) {
        for (CNSocket *s : chunk->players) {
            Player *plr = s->plr;

            if (plr->HP <= 0)
                continue;

            int mobRange = mob->sightRange;

            if (plr->iConditionBitFlag & CSB_BIT_UP_STEALTH)
                mobRange /= 3;

            if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVISIBLE)
                mobRange = -1;

            // height is relevant for aggro distance because of platforming
            int xyDistance = hypot(mob->appearanceData.iX - plr->x, mob->appearanceData.iY - plr->y);
            int distance = hypot(xyDistance, mob->appearanceData.iZ - plr->z);

            if (distance > mobRange || distance > closestDistance)
                continue;

            // found a player
            closest = s;
            closestDistance = distance;
        }
    }

    if (closest != nullptr) {
        // found closest player. engage.
        mob->target = closest;
        mob->state = MobState::COMBAT;
        mob->nextMovement = currTime;
        mob->nextAttack = 0;

        mob->roamX = mob->appearanceData.iX;
        mob->roamY = mob->appearanceData.iY;
        mob->roamZ = mob->appearanceData.iZ;

        return true;
    }

    return false;
}
