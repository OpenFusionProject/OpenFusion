#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "CombatManager.hpp"

void NanoManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO, nanoGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
}

void NanoManager::nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_EQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_EQUIP* nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_EQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
        return;

    resp.iNanoID = nano->iNanoID;
    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // Update player
    plr->equippedNanos[nano->iNanoSlotNum] = nano->iNanoID;

    // unsummon nano if replaced
    if (plr->activeNano == plr->equippedNanos[nano->iNanoSlotNum])
        summonNano(sock, -1);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_EQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC));
}

void NanoManager::nanoUnEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_UNEQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_UNEQUIP* nano = (sP_CL2FE_REQ_NANO_UNEQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_UNEQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
        return;

    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // unsummon nano if removed
    if (plr->equippedNanos[nano->iNanoSlotNum] == plr->activeNano)
        summonNano(sock, -1);

    // update player
    plr->equippedNanos[nano->iNanoSlotNum] = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_UNEQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_UNEQUIP_SUCC));
}

void NanoManager::nanoGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO))
        return; // ignore the malformed packet

    // Cmd: /nano <nanoId>
    sP_CL2FE_REQ_PC_GIVE_NANO* nano = (sP_CL2FE_REQ_PC_GIVE_NANO*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // Add nano to player
    addNano(sock, nano->iNanoID, 0);

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to add nano id: " << nano->iNanoID << std::endl;
    )
}

void NanoManager::nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_ACTIVE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_ACTIVE* pkt = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    summonNano(sock, pkt->iNanoSlotNum);

    // Send to client
    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    //if (data->size != sizeof(sP_CL2FE_REQ_NANO_SKILL_USE))
    //    return; // malformed packet
    
    Player *plr = PlayerManager::getPlayer(sock);
    int16_t nanoId = plr->activeNano;
    int skillId = plr->Nanos[nanoId].iSkillID;
    
    if (skillId == 1)
        nanoStun(sock, data, nanoId, skillId);
    
    if (skillId == 2)
        nanoHeal(sock, data, nanoId, skillId);

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano skill " << std::endl;
    )
}

void NanoManager::nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_TUNE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skill->iNanoID, skill->iTuneID);
}

#pragma region Helper methods
void NanoManager::addNano(CNSocket* sock, int16_t nanoId, int16_t slot) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoId;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC, sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC));

    // Update player
    plr->Nanos[nanoId] = resp.Nano;
    
    // After a nano gets added, setting the level seems to be important so we are doing that
    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_LEVEL, resp2);

    resp2.iPC_ID = plr->iID;
    resp2.iPC_Level = 36;

    sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
    plr->level = nanoId;
}

void NanoManager::summonNano(CNSocket *sock, int slot) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "summon nano\n";
    
    struct sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT {
	int32_t eCT;
	int32_t iID;
	int32_t iConditionBitFlag;
    };

    if (slot > 2 || slot < -1)
        return; //sanity check

    int nanoId = plr->equippedNanos[slot];

    if (nanoId > 36 || nanoId < 0)
        return; // sanity check
    
    int skillId = 0;
    
    if (plr->activeNano > 0) {
        skillId = plr->Nanos[plr->activeNano].iSkillID;
        if (skillId == 4) {
            INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp1);
    
            resp1.eCSTB = 1; //eCharStatusTimeBuffID
            resp1.eTBU = 2; //eTimeBuffUpdate
            resp1.eTBT = 1; //eTimeBuffType 1 means nano
            resp1.TimeBuff.iValue = 150; //adds up to a total of 750(600+150)
            resp1.iConditionBitFlag = 0;
    
            sock->sendPacket((void*)&resp1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        }
    }

    sNano nano = plr->Nanos[nanoId];
    skillId = nano.iSkillID;
    
    if (slot > -1) {
        plr->activeNano = nanoId;
        
        if (skillId == 3) {
            nanoScavenge(sock, nanoId, skillId);
            resp.eCSTB___Add = 1;
        }
        
        if (skillId == 4) {
            nanoRun(sock, nanoId, skillId);
            resp.eCSTB___Add = 1;
        }
    } else
        plr->activeNano = 0;
    
    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));
    
    // Send to other players
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);

    pkt1.iPC_ID = plr->iID;
    pkt1.Nano = nano;
    
    sock->sendPacket((void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));
}

void NanoManager::setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);
    sNano nano = plr->Nanos[nanoId];

    nano.iSkillID = skillId;
    plr->Nanos[nanoId] = nano;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_NANO_TUNE_SUCC, resp);
    resp.iNanoID = nanoId;
    resp.iSkillID = skillId;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " set skill id " << skillId << " for nano: " << nanoId << std::endl;
    )
}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoId) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);
    sNano nano = plr->Nanos[nanoId];

    // 0 is reset
    nano.iSkillID = 0;
    plr->Nanos[nanoId] = nano;
}

// this is where the fun begins
void NanoManager::nanoStun(CNSocket* sock, CNPacketData* data, int16_t nanoId, int skillId) {
    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size\n";
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));

    /*
     * Due to the possibility of multiplication overflow (and regular buffer overflow),
     * both incoming and outgoing variable-length packets must be validated, at least if
     * the number of trailing structs isn't well known (ie. it's from the client).
     */
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(sSkillResult_Damage_N_Debuff))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + pkt->iTargetCnt * sizeof(sSkillResult_Damage_N_Debuff);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Damage_N_Debuff *respdata = (sSkillResult_Damage_N_Debuff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 8; //stun enum TODO: Send these enums to Defines
    resp->iTargetCnt = pkt->iTargetCnt;
        
    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] nanoStun: mob ID not found" << std::endl;
            return;
        }
        BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
        
        respdata[i].eCT = 4;
        respdata[i].iID = mob.appearanceData.iNPC_ID;
        respdata[i].iHP = mob.appearanceData.iHP;
        respdata[i].iConditionBitFlag = 512;
        std::cout << (int)mob.appearanceData.iNPC_ID << " was stunned" << std::endl;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoHeal(CNSocket* sock, CNPacketData* data, int16_t nanoId, int skillId) {
    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size\n";
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));

    /*
     * Due to the possibility of multiplication overflow (and regular buffer overflow),
     * both incoming and outgoing variable-length packets must be validated, at least if
     * the number of trailing structs isn't well known (ie. it's from the client).
     */
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(sSkillResult_Heal_HP))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + pkt->iTargetCnt * sizeof(sSkillResult_Heal_HP);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Heal_HP *respdata = (sSkillResult_Heal_HP*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 2; //heal enum TODO: Send these enums to Defines
    resp->iTargetCnt = pkt->iTargetCnt;
        
    for (int i = 0; i < pkt->iTargetCnt; i++) {    
        for (auto pair : PlayerManager::players) {
            if (pair.second.plr->iID == pktdata[i]) { 
                Player* plr = pair.second.plr;
                
                plr->HP += 300;
                
                respdata[i].eCT = 1;
                respdata[i].iID = plr->iID;
                respdata[i].iHP = plr->HP;
                respdata[i].iHealHP = 300;
                std::cout << (int)plr->iID << " was healed" << std::endl;
            }
        }   
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoScavenge(CNSocket* sock, int16_t nanoId, int skillId) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), 1, sizeof(sSkillResult_Buff))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Buff);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 20; //scavenge enum TODO: Send these enums to Defines
    resp->iTargetCnt = 1;
        
    respdata[1].eCT = 1;
    respdata[1].iID = plr->iID;
    respdata[1].iConditionBitFlag = 16384;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoRun(CNSocket* sock, int16_t nanoId, int skillId) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), 1, sizeof(sSkillResult_Buff))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Buff);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 11; //run enum TODO: Send these enums to Defines
    resp->iTargetCnt = 1;
        
    // this looks stupid but in the future there will be more cnts
    for (int i = 0; i < 1; i++) {
        respdata[i].eCT = 1;
        respdata[i].iID = plr->iID;
        respdata[i].iConditionBitFlag = 1;
        
        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt1);
    
        pkt1.eCSTB = 1; //eCharStatusTimeBuffID
        pkt1.eTBU = 1; //eTimeBuffUpdate
        pkt1.eTBT = 1; //eTimeBuffType 1 means nano
        pkt1.TimeBuff.iValue = 150; //adds up to a total of 750(600+150)
        pkt1.iConditionBitFlag = 1;
    
        sock->sendPacket((void*)&pkt1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}
#pragma endregion
