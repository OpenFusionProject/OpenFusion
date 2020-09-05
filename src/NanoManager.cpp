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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_WARP_USE_RECALL, nanoRecallHandler);
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
    int16_t skillId = plr->Nanos[nanoId].iSkillID;
    
    if (skillId == 1 || skillId == 13 || skillId == 42 || skillId == 59 || skillId == 78 || skillId == 103)
        nanoDebuff(sock, data, nanoId, skillId, 8, 512); // Stun
    else if (skillId == 2 || skillId == 7 || skillId == 12 || skillId == 38 || skillId == 53 || skillId == 61 || skillId == 82 || skillId == 92 ||  skillId == 98 )
        nanoHeal(sock, data, nanoId, skillId, 333); // Heal
    else if (skillId == 5 || skillId == 25 || skillId == 66 || skillId == 69 || skillId == 75 || skillId == 87) {
        // Recall
    } else if (skillId == 10 || skillId == 34 || skillId == 37 || skillId == 56 || skillId == 93 || skillId == 97)
        nanoDebuff(sock, data, nanoId, skillId, 3, 262144); // Drain
    else if (skillId == 17 || skillId == 18 || skillId == 27 || skillId == 41 || skillId == 43 || skillId == 47 || skillId == 90 || skillId == 96 || skillId == 106)
        nanoDebuff(sock, data, nanoId, skillId, 5, 128); // Snare
    else if (skillId == 19 || skillId == 21 || skillId == 33 || skillId == 45 || skillId == 46 || skillId == 52 || skillId == 101 || skillId == 105 || skillId == 108)
        nanoDamage(sock, data, nanoId, skillId, 133); // Damage
    else if (skillId == 20 || skillId == 63 || skillId == 91) {
        // Group Revive
    } else if (skillId == 24 || skillId == 51 || skillId == 89) {
        nanoLeech(sock, data, nanoId, skillId, 133); // Leech
    } else if (skillId == 28 || skillId == 30 || skillId == 32 || skillId == 49 || skillId == 70 || skillId == 71 || skillId == 81 || skillId == 85 || skillId == 94)
        nanoDebuff(sock, data, nanoId, skillId, 4, 1024); // Sleep

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

void NanoManager::nanoRecallHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_WARP_USE_RECALL))
        return;
    
    INITSTRUCT(sP_FE2CL_REP_WARP_USE_RECALL_FAIL, resp);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_WARP_USE_RECALL_FAIL, sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL));
    // stubbed for now
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
    resp2.iPC_Level = nanoId;

    sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
    
    plr->level = nanoId;
    plr->iConditionBitFlag = 0;
}

void NanoManager::summonNano(CNSocket *sock, int slot) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "summon nano\n";

    if (slot > 2 || slot < -1)
        return; //sanity check

    int16_t nanoId = plr->equippedNanos[slot];

    if (nanoId > 36 || nanoId < 0)
        return; // sanity check
    
    int16_t skillId = 0;
    
    if (plr->activeNano > 0) {
        skillId = plr->Nanos[plr->activeNano].iSkillID;
        if (skillId == 3 || skillId == 50 || skillId == 99)
            nanoUnbuff(sock, 16384, 15); // Scavenge
        else if (skillId == 4 || skillId == 8 || skillId == 62 || skillId == 68 || skillId == 73 || skillId == 86)
            nanoUnbuff(sock, 1, 1, 200); // Run
        else if (skillId == 6 || skillId == 54 || skillId == 104)
            nanoUnbuff(sock, 32768, 16); // Bonus
        else if (skillId == 9 || skillId == 57 || skillId == 76)
            nanoUnbuff(sock, 32, 6); // Guard
        else if (skillId == 11 || skillId == 67 || skillId == 95)
            nanoUnbuff(sock, 4096, 13); // Radar
        else if (skillId == 14 || skillId == 58 || skillId == 102)
            nanoUnbuff(sock, 64, 7); // Antidote
        else if (skillId == 15 || skillId == 31 || skillId == 39 || skillId == 55 || skillId == 77 || skillId == 107)
            nanoUnbuff(sock, 131072, 18); // Freedom
        else if (skillId == 16 || skillId == 35 || skillId == 44 || skillId == 60 || skillId == 88 || skillId == 100)
            nanoUnbuff(sock, 4, 3, 400); // Jump
        else if (skillId == 22 || skillId == 48 || skillId == 83)
            nanoUnbuff(sock, 16, 5); // Self Revive
        else if (skillId == 23 || skillId == 29 || skillId == 65 || skillId == 72 || skillId == 80 || skillId == 82)
            nanoUnbuff(sock, 8, 4); // Sneak
        else if (skillId == 26 || skillId == 40 || skillId == 74)
            nanoUnbuff(sock, 8192, 14); // Treasure Finder
    }

    sNano nano = plr->Nanos[nanoId];
    skillId = nano.iSkillID;
    
    if (slot > -1) {
        plr->activeNano = nanoId;
        
        if (skillId == 3 || skillId == 50 || skillId == 99) {
            nanoBuff(sock, nanoId, skillId, 19, 16384, 15); // Scavenge
            resp.eCSTB___Add = 1;
        } else if (skillId == 4 || skillId == 8 || skillId == 62 || skillId == 68 || skillId == 73 || skillId == 86) {
            nanoBuff(sock, nanoId, skillId, 11, 1, 1, 200); // Run
            resp.eCSTB___Add = 1;
        } else if (skillId == 6 || skillId == 54 || skillId == 104) {
            nanoBuff(sock, nanoId, skillId, 20, 32768, 16); // Bonus
            resp.eCSTB___Add = 1;
        } else if (skillId == 9 || skillId == 57 || skillId == 76) {
            nanoBuff(sock, nanoId, skillId, 17, 32, 6); // Guard
            resp.eCSTB___Add = 1;
        } else if (skillId == 11 || skillId == 67 || skillId == 95) {
            nanoBuff(sock, nanoId, skillId, 14, 4096, 13); // Radar
            resp.eCSTB___Add = 1;
        } else if (skillId == 14 || skillId == 58 || skillId == 102) {
            nanoBuff(sock, nanoId, skillId, 18, 64, 7); // Antidote
            resp.eCSTB___Add = 1;
        } else if (skillId == 15 || skillId == 31 || skillId == 39 || skillId == 55 || skillId == 77 || skillId == 107) {
            nanoBuff(sock, nanoId, skillId, 25, 131072, 18); // Freedom
            resp.eCSTB___Add = 1;
        } else if (skillId == 16 || skillId == 35 || skillId == 44) {
            nanoBuff(sock, nanoId, skillId, 10, 4, 3, 400); // Jump
            resp.eCSTB___Add = 1;
        } else if (skillId == 22 || skillId == 48 || skillId == 83) {
            nanoBuff(sock, nanoId, skillId, 16, 16, 5); // Self Revive
            resp.eCSTB___Add = 1;
        } else if (skillId == 23 || skillId == 29 || skillId == 65 || skillId == 72 || skillId == 80 || skillId == 82) {
            nanoBuff(sock, nanoId, skillId, 12, 8, 4); // Sneak
            resp.eCSTB___Add = 1;
        } else if (skillId == 26 || skillId == 40 || skillId == 74) {
            nanoBuff(sock, nanoId, skillId, 15, 8192, 14); // Treasure Finder
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
void NanoManager::nanoDebuff(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t eSkillType, int32_t iCBFlag, int32_t damageAmount) {
    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

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
        
    Player *plr = PlayerManager::getPlayer(sock);
    
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = eSkillType;
    resp->iTargetCnt = pkt->iTargetCnt;
        
    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
            return;
        }
        BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
        
        mob.appearanceData.iHP -= damageAmount;

        if (mob.appearanceData.iHP <= 0)
            CombatManager::giveReward(sock);
        
        respdata[i].eCT = 4;
        respdata[i].iDamage = damageAmount;
        respdata[i].iID = mob.appearanceData.iNPC_ID;
        respdata[i].iHP = mob.appearanceData.iHP;
        respdata[i].iConditionBitFlag = iCBFlag;
        std::cout << (int)mob.appearanceData.iNPC_ID << " was debuffed" << std::endl;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoHeal(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int16_t healAmount) {
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
                               
                if (plr->HP + healAmount > 3625)
                    plr->HP = 3625;
                else
                    plr->HP += healAmount;
                
                respdata[i].eCT = 1;
                respdata[i].iID = plr->iID;
                respdata[i].iHP = plr->HP;
                respdata[i].iHealHP = healAmount;
                
                std::cout << (int)plr->iID << " was healed" << std::endl;
            }
        }   
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue) {
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
    resp->eST = eSkillType; //run enum TODO: Send these enums to Defines
    resp->iTargetCnt = 1;
        
    // this looks stupid but in the future there will be more counts (for group powers)
    for (int i = 0; i < 1; i++) {
        
        plr->iConditionBitFlag += iCBFlag;
        
        respdata[i].eCT = 1;
        respdata[i].iID = plr->iID;
        respdata[i].iConditionBitFlag = iCBFlag;
        
        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt1);
    
        pkt1.eCSTB = eCharStatusTimeBuffID; //eCharStatusTimeBuffID
        pkt1.eTBU = 1; //eTimeBuffUpdate
        pkt1.eTBT = 1; //eTimeBuffType 1 means nano
        pkt1.iConditionBitFlag = plr->iConditionBitFlag;
        
        if (iValue > 0)
            pkt1.TimeBuff.iValue = iValue;
    
        sock->sendPacket((void*)&pkt1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoUnbuff(CNSocket* sock, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue) {
    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp1);
    
    Player *plr = PlayerManager::getPlayer(sock);
    if (iCBFlag < plr->iConditionBitFlag) // prevents integer underflow
        plr->iConditionBitFlag -= iCBFlag;
    else
        plr->iConditionBitFlag = 0;
    
    resp1.eCSTB = eCharStatusTimeBuffID; //eCharStatusTimeBuffID
    resp1.eTBU = 2; //eTimeBuffUpdate
    resp1.eTBT = 1; //eTimeBuffType 1 means nano
    resp1.iConditionBitFlag = plr->iConditionBitFlag;
    
    if (iValue > 0)
        resp1.TimeBuff.iValue = iValue;
    
    sock->sendPacket((void*)&resp1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

void NanoManager::nanoDamage(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int32_t damageAmount) {
    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

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
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(sSkillResult_Damage))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + pkt->iTargetCnt * sizeof(sSkillResult_Damage);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Damage *respdata = (sSkillResult_Damage*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    Player *plr = PlayerManager::getPlayer(sock);
    
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 1;
    resp->iTargetCnt = pkt->iTargetCnt;
        
    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
            return;
        }
        BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
        
        mob.appearanceData.iHP -= damageAmount;

        if (mob.appearanceData.iHP <= 0)
            CombatManager::giveReward(sock);
        
        respdata[i].eCT = 4;
        respdata[i].iDamage = damageAmount;
        respdata[i].iID = mob.appearanceData.iNPC_ID;
        respdata[i].iHP = mob.appearanceData.iHP;
        std::cout << (int)mob.appearanceData.iNPC_ID << " was damaged" << std::endl;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoLeech(CNSocket* sock, CNPacketData* data, int16_t nanoId, int16_t skillId, int32_t leechAmount) {
    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

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
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Heal_HP), pkt->iTargetCnt, sizeof(sSkillResult_Damage))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Heal_HP) + pkt->iTargetCnt * sizeof(sSkillResult_Damage);
    uint8_t respbuf[4096];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Heal_HP *resp2 = (sSkillResult_Heal_HP*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
    sSkillResult_Damage *respdata = (sSkillResult_Damage*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE)+sizeof(sSkillResult_Heal_HP));
        
    Player *plr = PlayerManager::getPlayer(sock);
    
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = 30;
    resp->iTargetCnt = pkt->iTargetCnt;
    
    if (plr->HP + leechAmount > 3625)
        plr->HP = 3625;
    else
        plr->HP += leechAmount;
                
    resp2->eCT = 1;
    resp2->iID = plr->iID;
    resp2->iHP = plr->HP;
    resp2->iHealHP = leechAmount;
        
    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
            return;
        }
        BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
        
        mob.appearanceData.iHP -= leechAmount;

        if (mob.appearanceData.iHP <= 0)
            CombatManager::giveReward(sock);
        
        respdata[i].eCT = 4;
        respdata[i].iDamage = leechAmount;
        respdata[i].iID = mob.appearanceData.iNPC_ID;
        respdata[i].iHP = mob.appearanceData.iHP;
        std::cout << (int)mob.appearanceData.iNPC_ID << " was damaged" << std::endl;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}
#pragma endregion
