#include "CombatManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"

#include <cstdio>

void CombatManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_NPCs, pcAttackNpcs);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_BEGIN, combatBegin);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_END, combatEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_DOT_DAMAGE_ONOFF, dotDamageOnOff);
}

void CombatManager::pcAttackNpcs(CNSocket *sock, CNPacketData *data) {
    // generic malformed packet checks are not applicable to variable-length packets
    sP_CL2FE_REQ_PC_ATTACK_NPCs* pkt = (sP_CL2FE_REQ_PC_ATTACK_NPCs*)data->buf;

    if (data->size != sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs) + pkt->iNPCCnt * 4) {
        std::cout << "bad sP_CL2FE_REQ_PC_ATTACK_NPCs packet size\n";
        return;
    }
    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs));

    std::printf("iNPCCnt: %d\n", pkt->iNPCCnt);

    // initialize response struct
    // IMPORTANT TODO: verify that resplen doesn't overflow!!!
    size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) + pkt->iNPCCnt * sizeof(sAttackResult);
    uint8_t *respbuf = (uint8_t*)xmalloc(resplen);
    memset(respbuf, 0, resplen);
    sP_FE2CL_PC_ATTACK_NPCs_SUCC *resp = (sP_FE2CL_PC_ATTACK_NPCs_SUCC*)respbuf;
    sAttackResult *respdata = (sAttackResult*)(respbuf+sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC));

    resp->iNPCCnt = pkt->iNPCCnt;

    for (int i = 0; i < pkt->iNPCCnt; i++) {
        std::cout << pktdata[i] << std::endl;

        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] pcAttackNpcs: mob ID not found" << std::endl;
            return;
        }
        BaseNPC& mob = NPCManager::NPCs[pktdata[i]];

        mob.appearanceData.iHP -= 100;
        std::cout << "mob health is now " << mob.appearanceData.iHP << std::endl;

        if (mob.appearanceData.iHP <= 0)
            giveReward(sock);

        respdata[i].iID = mob.appearanceData.iNPC_ID;
        respdata[i].iDamage = 100;
        respdata[i].iHP = mob.appearanceData.iHP;
        respdata[i].iHitFlag = 2;
    }

    std::cout << "sending packet of length " << resplen << std::endl;
    sock->sendPacket((void*)respbuf, P_FE2CL_PC_ATTACK_NPCs_SUCC, resplen);

    free(respbuf);
}

void CombatManager::combatBegin(CNSocket *sock, CNPacketData *data) {} // stub
void CombatManager::combatEnd(CNSocket *sock, CNPacketData *data) {} // stub
void CombatManager::dotDamageOnOff(CNSocket *sock, CNPacketData *data) {} // stub

void CombatManager::giveReward(CNSocket *sock) {
    // reward testing
    INITSTRUCT(sP_FE2CL_REP_REWARD_ITEM, reward);
    Player *plr = PlayerManager::getPlayer(sock);

    // update player
    plr->money += 50;
    plr->fusionmatter += 70;

    reward.m_iCandy = plr->money;
    reward.m_iFusionMatter = plr->fusionmatter;
    reward.iFatigue = 100; // prevents warning message
    reward.iFatigue_Level = 1;

    sock->sendPacket((void*)&reward, P_FE2CL_REP_REWARD_ITEM, sizeof(sP_FE2CL_REP_REWARD_ITEM));
}
