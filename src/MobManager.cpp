#include "MobManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"

#include <assert.h>

std::map<int32_t, Mob*> MobManager::Mobs;

void MobManager::init() {
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

        mob->appearanceData.iHP -= 100;

        if (mob->appearanceData.iHP <= 0)
            killMob(sock, mob);

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
    plr->fusionmatter += 70;

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
    mob->killedTime = getTime(); // XXX: maybe introduce a shard-global time for each step?

    std::cout << "killed mob " << mob->appearanceData.iNPC_ID << std::endl;

    giveReward(sock);
    MissionManager::mobKilled(sock, mob->appearanceData.iNPCType);

    INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);

    pkt.iNPC_ID = mob->appearanceData.iNPC_ID;

    sock->sendPacket(&pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
    PlayerManager::sendToViewable(sock, (void*)&pkt, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
}

void MobManager::deadStep(Mob *mob, time_t currTime) {
    if (mob->killedTime != 0 && currTime - mob->killedTime < mob->regenTime * 100)
        return;

    std::cout << "respawning mob " << mob->appearanceData.iNPC_ID << " with HP = " << mob->maxHealth << std::endl;

    mob->appearanceData.iHP = mob->maxHealth;
    mob->state = MobState::ROAMING;

    INITSTRUCT(sP_FE2CL_NPC_NEW, pkt);

    pkt.NPCAppearanceData = mob->appearanceData;

    // FIXME: use the chunk's visibility list, when that becomes a thing
    for (auto& pair : PlayerManager::players) {
        Player *plr = pair.second.plr;

        int diffX = abs(plr->x - mob->appearanceData.iX);
        int diffY = abs(plr->y - mob->appearanceData.iY);

        if (diffX < settings::PLAYERDISTANCE && diffY < settings::PLAYERDISTANCE)
            pair.first->sendPacket(&pkt, P_FE2CL_NPC_NEW, sizeof(sP_FE2CL_NPC_NEW));
    }
}

void MobManager::step(time_t currTime) {
    for (auto& pair : Mobs) {
        switch (pair.second->state) {
        case MobState::DEAD:
            deadStep(pair.second, currTime);
            break;
        default:
            // unhandled for now
            break;
        }
    }
}
