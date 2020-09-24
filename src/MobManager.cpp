#include "MobManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"

#include <cmath>
#include <assert.h>

std::map<int32_t, Mob*> MobManager::Mobs;

void MobManager::init() {
    REGISTER_SHARD_TIMER(step, 200);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_NPCs, pcAttackNpcs);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_BEGIN, combatBegin);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_END, combatEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_DOT_DAMAGE_ONOFF, dotDamageOnOff);
}

void MobManager::pcAttackNpcs(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_REQ_PC_ATTACK_NPCs* pkt = (sP_CL2FE_REQ_PC_ATTACK_NPCs*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

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

        // cannot kill mobs multiple times; cannot harm retreating mobs
        if (mob->state != MobState::ROAMING && mob->state != MobState::COMBAT) {
            respdata[i].iID = mob->appearanceData.iNPC_ID;
            respdata[i].iHP = mob->appearanceData.iHP;
            respdata[i].iHitFlag = 2; // probably still necessary
            continue;
        }

        if (mob->state == MobState::ROAMING) {
            assert(mob->target == nullptr);
            mob->target = sock;
            mob->state = MobState::COMBAT;
            mob->nextMovement = getTime();
            std::cout << "combat state\n";
        }

        mob->appearanceData.iHP -= 100;

        // wake up sleeping monster
        // TODO: remove client-side bit somehow
        mob->appearanceData.iConditionBitFlag &= ~CSB_BIT_MEZ;

        if (mob->appearanceData.iHP <= 0)
            killMob(mob->target, mob);

        respdata[i].iID = mob->appearanceData.iNPC_ID;
        respdata[i].iDamage = 100;
        respdata[i].iHP = mob->appearanceData.iHP;
        respdata[i].iHitFlag = 2; // hitscan, not a rocket or a grenade
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_PC_ATTACK_NPCs_SUCC, resplen);

    // a bit of a hack: these are the same size, so we can reuse the response packet
    assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_NPCs));
    sP_FE2CL_PC_ATTACK_NPCs *resp1 = (sP_FE2CL_PC_ATTACK_NPCs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_ATTACK_NPCs, resplen);
}

void MobManager::npcAttackPc(Mob *mob) {
    // player pointer has already been validated
    Player *plr = PlayerManager::getPlayer(mob->target);

    const size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) + sizeof(sAttackResult);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_ATTACK_PCs *pkt = (sP_FE2CL_NPC_ATTACK_PCs*)respbuf;
    sAttackResult *atk = (sAttackResult*)(respbuf + sizeof(sP_FE2CL_NPC_ATTACK_PCs));

    plr->HP += (int)mob->data["m_iPower"]; // already negative

    pkt->iNPC_ID = mob->appearanceData.iNPC_ID;
    pkt->iPCCnt = 1;

    atk->iID = plr->iID;
    atk->iDamage = -(int)mob->data["m_iPower"];
    atk->iHP = plr->HP;
    atk->iHitFlag = 2;

    mob->target->sendPacket((void*)respbuf, P_FE2CL_NPC_ATTACK_PCs, resplen);
    PlayerManager::sendToViewable(mob->target, (void*)&pkt, P_FE2CL_NPC_ATTACK_PCs, resplen);

    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        std::cout << "retreat state\n";
    }
}

void MobManager::combatBegin(CNSocket *sock, CNPacketData *data) {} // stub
void MobManager::combatEnd(CNSocket *sock, CNPacketData *data) {} // stub
void MobManager::dotDamageOnOff(CNSocket *sock, CNPacketData *data) {} // stub

void MobManager::giveReward(CNSocket *sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // update player
    plr->money += 50;
    MissionManager::updateFusionMatter(sock, 70);

    // simple rewards
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;
    reward->iItemCnt = 1; // remember to update resplen if you change this

    int slot = ItemManager::findFreeSlot(plr);
    if (slot == -1) {
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

void MobManager::killMob(CNSocket *sock, Mob *mob) {
    mob->state = MobState::DEAD;
    mob->target = nullptr;
    mob->appearanceData.iConditionBitFlag = 0;
    mob->killedTime = getTime(); // XXX: maybe introduce a shard-global time for each step?

    std::cout << "dead state mob " << mob->appearanceData.iNPC_ID << std::endl;

    giveReward(sock);
    MissionManager::mobKilled(sock, mob->appearanceData.iNPCType);

    mob->despawned = false;
}

void MobManager::deadStep(Mob *mob, time_t currTime) {
    auto chunk = ChunkManager::grabChunk(mob->appearanceData.iX, mob->appearanceData.iY);
    auto chunks = ChunkManager::grabChunks(chunk);

    // despawn the mob after a short delay
    if (mob->killedTime != 0 && !mob->despawned && currTime - mob->killedTime > 2000) {
        mob->despawned = true;

        INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;

        for (Chunk *chunk : chunks) {
            for (CNSocket *s : chunk->players) {
                s->sendPacket(&pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
            }
        }
    }

    if (mob->killedTime != 0 && currTime - mob->killedTime < mob->regenTime * 100)
        return;

    std::cout << "respawning mob " << mob->appearanceData.iNPC_ID << " with HP = " << mob->maxHealth << std::endl;

    mob->appearanceData.iHP = mob->maxHealth;
    mob->state = MobState::ROAMING;
    std::cout << "roaming state\n";

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = mob->appearanceData;

    // notify all nearby players
    for (Chunk *chunk : chunks) {
        for (CNSocket *s : chunk->players) {
            s->sendPacket(&pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
        }
    }
}

void MobManager::combatStep(Mob *mob, time_t currTime) {
    assert(mob->target != nullptr);

    // sanity check: did the target player lose connection?
    if (PlayerManager::players.find(mob->target) == PlayerManager::players.end()) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        std::cout << "RETREAT state\n";
        return;
    }
    Player *plr = PlayerManager::getPlayer(mob->target);

    // skip attack/move if stunned or asleep
    if (mob->appearanceData.iConditionBitFlag & (CSB_BIT_STUN|CSB_BIT_MEZ))
        return;

    int distance = hypot(plr->x - mob->appearanceData.iX, plr->y - mob->appearanceData.iY);

    /*
     * If the mob is close enough to attack, do so. If not, get closer.
     * No, I'm not 100% sure this is how it's supposed to work.
     */
    if (distance <= (int)mob->data["m_iAtkRange"]) {
        std::cout << "attack logic\n";
        // attack logic
        if (mob->nextAttack == 0) {
            mob->nextAttack = currTime + (int)mob->data["m_iInitalTime"] * 100; // I *think* this is what this is
            npcAttackPc(mob);
        } else if (mob->nextAttack != 0 && currTime >= mob->nextAttack) {
            mob->nextAttack = currTime + (int)mob->data["m_iDelayTime"] * 100;
            npcAttackPc(mob);
        }
    } else {
        std::cout << "movement logic\n";
        std::cout << "distance: " << distance << " attack range: " << mob->data["m_iAtkRange"] << std::endl;
        // movement logic
        if (mob->nextMovement != 0 && currTime < mob->nextMovement)
            return;
        mob->nextMovement = currTime + (int)mob->data["m_iDelayTime"] * 100;

        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = mob->data["m_iWalkSpeed"];
        pkt.iToX = mob->appearanceData.iX = mob->target->plr->x; // FIXME: lerping
        pkt.iToY = mob->appearanceData.iY = mob->target->plr->y;
        pkt.iToZ = mob->appearanceData.iZ = mob->target->plr->z;

        auto chunk = ChunkManager::grabChunk(mob->appearanceData.iX, mob->appearanceData.iY);
        auto chunks = ChunkManager::grabChunks(chunk);

        // notify all nearby players
        for (Chunk *chunk : chunks) {
            for (CNSocket *s : chunk->players) {
                s->sendPacket(&pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
            }
        }
    }

    // TODO: retreat if kited too far
}

void MobManager::roamingStep(Mob *mob, time_t currTime) {
    if (mob->nextMovement != 0 && currTime < mob->nextMovement)
        return;

    int delay = (int)mob->data["m_iDelayTime"] * 1000;
    mob->nextMovement = currTime + delay/2 + rand() % (delay/2);

    // skip move if stunned or asleep
    if (mob->appearanceData.iConditionBitFlag & (CSB_BIT_STUN|CSB_BIT_MEZ))
        return;

    INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);
    int xStart = mob->spawnX - mob->idleRange/2;
    int yStart = mob->spawnY - mob->idleRange/2;

    pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
    pkt.iSpeed = mob->data["m_iWalkSpeed"];
    pkt.iToX = mob->appearanceData.iX = xStart + rand() % mob->idleRange;
    pkt.iToY = mob->appearanceData.iY = yStart + rand() % mob->idleRange;
    pkt.iToZ = mob->appearanceData.iZ;

    // roughly halve movement speed if snared
    // TODO: this logic isn't quite right yet, since they still get to the same spot
    if (mob->appearanceData.iConditionBitFlag & CSB_BIT_DN_MOVE_SPEED) {
        mob->nextMovement += delay;
        pkt.iSpeed /= 2;
    }

    auto chunk = ChunkManager::grabChunk(mob->appearanceData.iX, mob->appearanceData.iY);
    auto chunks = ChunkManager::grabChunks(chunk);

    // notify all nearby players
    for (Chunk *chunk : chunks) {
        for (CNSocket *s : chunk->players) {
            s->sendPacket(&pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
        }
    }
}

void MobManager::retreatStep(Mob *mob, time_t currTime) {
    // distance between spawn point and current location
    int distance = hypot(mob->appearanceData.iX - mob->spawnX, mob->appearanceData.iY - mob->spawnY);

    if (distance > mob->data["m_iIdleRange"]) {
        INITSTRUCT(sP_FE2CL_NPC_MOVE, pkt);

        auto chunk = ChunkManager::grabChunk(mob->appearanceData.iX, mob->appearanceData.iY);
        auto chunks = ChunkManager::grabChunks(chunk);

        pkt.iNPC_ID = mob->appearanceData.iNPC_ID;
        pkt.iSpeed = mob->data["m_iWalkSpeed"];
        pkt.iToX = mob->appearanceData.iX = mob->spawnX;
        pkt.iToY = mob->appearanceData.iY = mob->spawnY;
        pkt.iToZ = mob->appearanceData.iZ;

        // notify all nearby players
        for (Chunk *chunk : chunks) {
            for (CNSocket *s : chunk->players) {
                s->sendPacket(&pkt, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
            }
        }
    }

    // if we got there
    if (distance <= mob->data["m_iIdleRange"]) {
        mob->state = MobState::ROAMING;
        std::cout << "roaming state\n";
        mob->appearanceData.iHP = mob->maxHealth;
        mob->killedTime = 0;
        mob->nextAttack = 0;
        mob->appearanceData.iConditionBitFlag = 0;
    }
}

void MobManager::step(CNServer *serv, time_t currTime) {
    for (auto& pair : Mobs) {
        int x = pair.second->appearanceData.iX;
        int y = pair.second->appearanceData.iY;

        // skip chunks without players
        if (!ChunkManager::inPopulatedChunks(x, y))
            continue;

        // skip mob movement and combat if disabled
        if (!settings::SIMULATEMOBS && pair.second->state != MobState::DEAD)
            continue;

        switch (pair.second->state) {
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
        default:
            // unhandled for now
            break;
        }
    }
}
