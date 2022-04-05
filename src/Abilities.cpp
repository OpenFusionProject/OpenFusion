#include "Abilities.hpp"
#include "PlayerManager.hpp"
#include "Player.hpp"
#include "NPCManager.hpp"
#include "Nanos.hpp"
#include "Groups.hpp"
#include "Eggs.hpp"

/*
 * TODO: This file is in desperate need of deduplication and rewriting.
 */

std::map<int32_t, SkillData> Nanos::SkillTable;

/*
 * targetData approach
 * first integer is the count
 * second to fifth integers are IDs, these can be either player iID or mob's iID
 */
std::vector<int> Nanos::findTargets(Player* plr, int skillID, CNPacketData* data) {
    std::vector<int> tD(5);

    if (SkillTable[skillID].targetType <= 2 && data != nullptr) { // client gives us the targets
        sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

        // validate request check
        if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
            std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size" << std::endl;
            return tD;
        }

        int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));
        tD[0] = pkt->iTargetCnt;

        for (int i = 0; i < pkt->iTargetCnt; i++)
            tD[i+1] = pktdata[i];

    } else if (SkillTable[skillID].targetType == 2) { // self target only
        tD[0] = 1;
        tD[1] = plr->iID;

    } else if (SkillTable[skillID].targetType == 3) { // entire group as target
        Player *otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

        if (otherPlr == nullptr)
            return tD;

        if (SkillTable[skillID].effectArea == 0) { // for buffs
            tD[0] = otherPlr->groupCnt;
            for (int i = 0; i < otherPlr->groupCnt; i++)
                tD[i+1] = otherPlr->groupIDs[i];
            return tD;
        }

        for (int i = 0; i < otherPlr->groupCnt; i++) { // group heals have an area limit
            Player *otherPlr2 = PlayerManager::getPlayerFromID(otherPlr->groupIDs[i]);
            if (otherPlr2 == nullptr)
                continue;
            if (true) {//hypot(otherPlr2->x - plr->x, otherPlr2->y - plr->y) < SkillTable[skillID].effectArea) {
                tD[i+1] = otherPlr->groupIDs[i];
                tD[0] += 1;
            }
        }
    }
    
    return tD;
}

void Nanos::nanoUnbuff(CNSocket* sock, std::vector<int> targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->iSelfConditionBitFlag &= ~bitFlag;
    int groupFlags = 0;

    if (groupPower) {
        plr->iGroupConditionBitFlag &= ~bitFlag;
        Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);
        if (leader != nullptr)
            groupFlags = Groups::getGroupFlags(leader);
    }

    for (int i = 0; i < targetData[0]; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(targetData[i+1]);
        if (!((groupFlags | varPlr->iSelfConditionBitFlag) & bitFlag)) {
            CNSocket* sockTo = PlayerManager::getSockFromID(targetData[i+1]);
            if (sockTo == nullptr)
                continue; // sanity check

            INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
            resp.eCSTB = timeBuffID; // eCharStatusTimeBuffID
            resp.eTBU = 2; // eTimeBuffUpdate
            resp.eTBT = 1; // eTimeBuffType 1 means nano
            varPlr->iConditionBitFlag &= ~bitFlag;
            resp.iConditionBitFlag = varPlr->iConditionBitFlag |= groupFlags | varPlr->iSelfConditionBitFlag;

            if (amount > 0)
                resp.TimeBuff.iValue = amount;

            sockTo->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        }
    }
}

int Nanos::applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags) {
    if (SkillTable[skillID].drainType == 1)
        return 0;

    int32_t bitFlag = 0;

    for (auto& pwr : NanoPowers) {
        if (pwr.skillType == SkillTable[skillID].skillType) {
            bitFlag = pwr.bitFlag;
            Player *plr = PlayerManager::getPlayer(sock);
            if (eTBU == 1 || !((groupFlags | plr->iSelfConditionBitFlag) & bitFlag)) {
                INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
                resp.eCSTB = pwr.timeBuffID;
                resp.eTBU = eTBU;
                resp.eTBT = eTBT;

                if (eTBU == 1)
                    plr->iConditionBitFlag |= bitFlag;
                else
                    plr->iConditionBitFlag &= ~bitFlag;

                resp.iConditionBitFlag = plr->iConditionBitFlag |= groupFlags | plr->iSelfConditionBitFlag;
                resp.TimeBuff.iValue = SkillTable[skillID].powerIntensity[0];
                sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
            }
            return bitFlag;
        }
    }

    return 0;
}

#pragma region Nano Powers
namespace Nanos {

bool doDebuff(CNSocket *sock, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (NPCManager::NPCs.find(targetID) == NPCManager::NPCs.end()) {
        std::cout << "[WARN] doDebuff: NPC ID not found" << std::endl;
        return false;
    }

    BaseNPC* npc = NPCManager::NPCs[targetID];
    if (npc->type != EntityType::MOB) {
        std::cout << "[WARN] doDebuff: NPC is not a mob" << std::endl;
        return false;
    }

    Mob* mob = (Mob*)npc;
    Combat::hitMob(sock, mob, 0);

    respdata[i].eCT = 4;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].bProtected = 1;
    if (mob->skillStyle < 0 && mob->state != MobState::RETREAT 
    && !(mob->appearanceData.iConditionBitFlag & CSB_BIT_FREEDOM)) { // only debuff if the enemy is not retreating, casting corruption or in freedom
        mob->appearanceData.iConditionBitFlag |= bitFlag;
        mob->unbuffTimes[bitFlag] = getTime() + duration * 100;
        respdata[i].bProtected = 0;
    }
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag;

    return true;
}

bool doBuff(CNSocket *sock, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;
    CNSocket *sockTo = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            sockTo = pair.first;
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (sockTo == nullptr || plr == nullptr) {
        std::cout << "[WARN] doBuff: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iConditionBitFlag = 0;

    // only apply buffs if the player is actually alive
    if (plr->HP > 0) {
        respdata[i].iConditionBitFlag = bitFlag;

        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
        pkt.eCSTB = timeBuffID; // eCharStatusTimeBuffID
        pkt.eTBU = 1; // eTimeBuffUpdate
        pkt.eTBT = 1; // eTimeBuffType 1 means nano
        pkt.iConditionBitFlag = plr->iConditionBitFlag |= bitFlag;

        if (amount > 0)
            pkt.TimeBuff.iValue = amount;

        sockTo->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    return true;
}

bool doDamageNDebuff(CNSocket *sock, sSkillResult_Damage_N_Debuff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (NPCManager::NPCs.find(targetID) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamageNDebuff: NPC ID not found" << std::endl;
        return false;
    }

    BaseNPC* npc = NPCManager::NPCs[targetID];
    if (npc->type != EntityType::MOB) {
        std::cout << "[WARN] doDamageNDebuff: NPC is not a mob" << std::endl;
        return false;
    }

    Mob* mob = (Mob*)npc;

    Combat::hitMob(sock, mob, 0); // just to gain aggro

    respdata[i].eCT = 4;
    respdata[i].iDamage = duration / 10;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;
    respdata[i].bProtected = 1;
    if (mob->skillStyle < 0 && mob->state != MobState::RETREAT 
    && !(mob->appearanceData.iConditionBitFlag & CSB_BIT_FREEDOM)) { // only debuff if the enemy is not retreating, casting corruption or in freedom
        mob->appearanceData.iConditionBitFlag |= bitFlag;
        mob->unbuffTimes[bitFlag] = getTime() + duration * 100;
        respdata[i].bProtected = 0;
    }
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag;

    return true;
}

bool doHeal(CNSocket *sock, sSkillResult_Heal_HP *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doHeal: player ID not found" << std::endl;
        return false;
    }

    int healedAmount = PC_MAXHEALTH(plr->level) * amount / 1000;

    // do not heal dead players
    if (plr->HP <= 0)
        healedAmount = 0;

    plr->HP += healedAmount;

    if (plr->HP > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP;
    respdata[i].iHealHP = healedAmount;

    return true;
}

bool doDamage(CNSocket *sock, sSkillResult_Damage *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (NPCManager::NPCs.find(targetID) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamage: NPC ID not found" << std::endl;
        return false;
    }

    BaseNPC* npc = NPCManager::NPCs[targetID];
    if (npc->type != EntityType::MOB) {
        std::cout << "[WARN] doDamage: NPC is not a mob" << std::endl;
        return false;
    }

    Mob* mob = (Mob*)npc;
    Player *plr = PlayerManager::getPlayer(sock);

    int damage = Combat::hitMob(sock, mob, std::max(PC_MAXHEALTH(plr->level) * amount / 1000, mob->maxHealth * amount / 1000));

    respdata[i].eCT = 4;
    respdata[i].iDamage = damage;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;

    return true;
}

/*
 * NOTE: Leech is specially encoded.
 *
 * It manages to fit inside the nanoPower<>() mold with only a slight hack,
 * but it really is it's own thing. There is a hard assumption that players
 * will only every leech a single mob, and the sanity check that enforces that
 * assumption is critical.
 */
 
bool doLeech(CNSocket *sock, sSkillResult_Heal_HP *healdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    // this sanity check is VERY important
    if (i != 0) {
        std::cout << "[WARN] Player attempted to leech more than one mob!" << std::endl;
        return false;
    }

    sSkillResult_Damage *damagedata = (sSkillResult_Damage*)(((uint8_t*)healdata) + sizeof(sSkillResult_Heal_HP));
    Player *plr = PlayerManager::getPlayer(sock);

    int healedAmount = amount;

    plr->HP += healedAmount;

    if (plr->HP > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);

    healdata->eCT = 1;
    healdata->iID = plr->iID;
    healdata->iHP = plr->HP;
    healdata->iHealHP = healedAmount;

    if (NPCManager::NPCs.find(targetID) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doLeech: NPC ID not found" << std::endl;
        return false;
    }
    
    BaseNPC* npc = NPCManager::NPCs[targetID];
    if (npc->type != EntityType::MOB) {
        std::cout << "[WARN] doLeech: NPC is not a mob" << std::endl;
        return false;
    }

    Mob* mob = (Mob*)npc;

    int damage = Combat::hitMob(sock, mob, amount * 2);

    damagedata->eCT = 4;
    damagedata->iDamage = damage;
    damagedata->iID = mob->appearanceData.iNPC_ID;
    damagedata->iHP = mob->appearanceData.iHP;

    return true;
}

bool doResurrect(CNSocket *sock, sSkillResult_Resurrect *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doResurrect: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iRegenHP = plr->HP;

    return true;
}

bool doMove(CNSocket *sock, sSkillResult_Move *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doMove: player ID not found" << std::endl;
        return false;
    }

    Player *plr2 = PlayerManager::getPlayer(sock);

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iMapNum = plr2->recallInstance;
    respdata[i].iMoveX = plr2->recallX;
    respdata[i].iMoveY = plr2->recallY;
    respdata[i].iMoveZ = plr2->recallZ;

    return true;
}

template<class sPAYLOAD,
         bool (*work)(CNSocket*,sPAYLOAD*,int,int32_t,int32_t,int16_t,int16_t,int16_t)>
void nanoPower(CNSocket *sock, std::vector<int> targetData,
                int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount, 
                int16_t skillType, int32_t bitFlag, int16_t timeBuffID) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (skillType == EST_RETROROCKET_SELF || skillType == EST_RECALL) // rocket and self recall does not need any trailing structs
        targetData[0] = 0;

    size_t resplen;
    // special case since leech is atypically encoded
    if (skillType == EST_BLOODSUCKING)
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + sizeof(sSkillResult_Heal_HP) + sizeof(sSkillResult_Damage);
    else
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + targetData[0] * sizeof(sPAYLOAD);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), targetData[0], sizeof(sPAYLOAD))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE_SUCC *resp = (sP_FE2CL_NANO_SKILL_USE_SUCC*)respbuf;
    sPAYLOAD *respdata = (sPAYLOAD*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));

    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillID;
    resp->iNanoID = nanoID;
    resp->iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
    resp->eST = skillType;
    resp->iTargetCnt = targetData[0];

    if (SkillTable[skillID].drainType == 2) {
        if (SkillTable[skillID].targetType >= 2)
            plr->iSelfConditionBitFlag |= bitFlag;
        if (SkillTable[skillID].targetType == 3)
            plr->iGroupConditionBitFlag |= bitFlag;
    }

    for (int i = 0; i < targetData[0]; i++)
        if (!work(sock, respdata, i, targetData[i+1], bitFlag, timeBuffID, duration, amount))
            return;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    assert(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) == sizeof(sP_FE2CL_NANO_SKILL_USE));
    if (skillType == EST_RECALL_GROUP) { // in the case of group recall, nobody but group members need the packet
        for (int i = 0; i < targetData[0]; i++) {
            CNSocket *sock2 = PlayerManager::getSockFromID(targetData[i+1]);
            sock2->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
        }
    } else
        PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);

    // Warping on recall
    if (skillType == EST_RECALL || skillType == EST_RECALL_GROUP) {
        if ((int32_t)plr->instanceID == plr->recallInstance)
            PlayerManager::sendPlayerTo(sock, plr->recallX, plr->recallY, plr->recallZ, plr->recallInstance);
        else {
            INITSTRUCT(sP_FE2CL_REP_WARP_USE_RECALL_FAIL, response)
            sock->sendPacket((void*)&response, P_FE2CL_REP_WARP_USE_RECALL_FAIL, sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL));
        }
    }
}

// nano power dispatch table
std::vector<NanoPower> NanoPowers = {
    NanoPower(EST_STUN,             CSB_BIT_STUN,              ECSB_STUN,              nanoPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    NanoPower(EST_HEAL_HP,          CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Heal_HP,                  doHeal>),
    NanoPower(EST_BOUNDINGBALL,     CSB_BIT_BOUNDINGBALL,      ECSB_BOUNDINGBALL,      nanoPower<sSkillResult_Buff,                   doDebuff>),
    NanoPower(EST_SNARE,            CSB_BIT_DN_MOVE_SPEED,     ECSB_DN_MOVE_SPEED,     nanoPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    NanoPower(EST_DAMAGE,           CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Damage,                 doDamage>),
    NanoPower(EST_BLOODSUCKING,     CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Heal_HP,                 doLeech>),
    NanoPower(EST_SLEEP,            CSB_BIT_MEZ,               ECSB_MEZ,               nanoPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    NanoPower(EST_REWARDBLOB,       CSB_BIT_REWARD_BLOB,       ECSB_REWARD_BLOB,       nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_RUN,              CSB_BIT_UP_MOVE_SPEED,     ECSB_UP_MOVE_SPEED,     nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_REWARDCASH,       CSB_BIT_REWARD_CASH,       ECSB_REWARD_CASH,       nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_PROTECTBATTERY,   CSB_BIT_PROTECT_BATTERY,   ECSB_PROTECT_BATTERY,   nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_MINIMAPENEMY,     CSB_BIT_MINIMAP_ENEMY,     ECSB_MINIMAP_ENEMY,     nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_PROTECTINFECTION, CSB_BIT_PROTECT_INFECTION, ECSB_PROTECT_INFECTION, nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_JUMP,             CSB_BIT_UP_JUMP_HEIGHT,    ECSB_UP_JUMP_HEIGHT,    nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_FREEDOM,          CSB_BIT_FREEDOM,           ECSB_FREEDOM,           nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_PHOENIX,          CSB_BIT_PHOENIX,           ECSB_PHOENIX,           nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_STEALTH,          CSB_BIT_UP_STEALTH,        ECSB_UP_STEALTH,        nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_MINIMAPTRESURE,   CSB_BIT_MINIMAP_TRESURE,   ECSB_MINIMAP_TRESURE,   nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_RECALL,           CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Move,                     doMove>),
    NanoPower(EST_RECALL_GROUP,     CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Move,                     doMove>),
    NanoPower(EST_RETROROCKET_SELF, CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_PHOENIX_GROUP,    CSB_BIT_NONE,              ECSB_NONE,              nanoPower<sSkillResult_Resurrect,           doResurrect>),
    NanoPower(EST_NANOSTIMPAK,      CSB_BIT_STIMPAKSLOT1,      ECSB_STIMPAKSLOT1,      nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_NANOSTIMPAK,      CSB_BIT_STIMPAKSLOT2,      ECSB_STIMPAKSLOT2,      nanoPower<sSkillResult_Buff,                     doBuff>),
    NanoPower(EST_NANOSTIMPAK,      CSB_BIT_STIMPAKSLOT3,      ECSB_STIMPAKSLOT3,      nanoPower<sSkillResult_Buff,                     doBuff>)
};

}; // namespace
#pragma endregion

#pragma region Mob Powers
namespace Combat {
bool doDamageNDebuff(Mob *mob, sSkillResult_Damage_N_Debuff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    CNSocket *sock = nullptr;
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            sock = pair.first;
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doDamageNDebuff: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iDamage = duration / 10;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP;
    respdata[i].iStamina = plr->Nanos[plr->activeNano].iStamina;
    if (plr->iConditionBitFlag & CSB_BIT_FREEDOM)
        respdata[i].bProtected = 1;
    else {
        if  (!(plr->iConditionBitFlag & bitFlag)) {
            INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
            pkt.eCSTB = timeBuffID; // eCharStatusTimeBuffID
            pkt.eTBU = 1; // eTimeBuffUpdate
            pkt.eTBT = 2;
            pkt.iConditionBitFlag = plr->iConditionBitFlag |= bitFlag;
            pkt.TimeBuff.iValue = amount * 5;
            sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        }

        respdata[i].bProtected = 0;
        std::pair<CNSocket*, int32_t> key = std::make_pair(sock, bitFlag);
        time_t until = getTime() + (time_t)duration * 100;
        Eggs::EggBuffs[key] = until;
    }
    respdata[i].iConditionBitFlag = plr->iConditionBitFlag;

    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        if (!MobAI::aggroCheck(mob, getTime())) {
            MobAI::clearDebuff(mob);
            if (mob->groupLeader != 0)
                MobAI::groupRetreat(mob);
        }
    }

    return true;
}

bool doHeal(Mob *mob, sSkillResult_Heal_HP *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (NPCManager::NPCs.find(targetID) == NPCManager::NPCs.end()) {
        std::cout << "[WARN] doHeal: NPC ID not found" << std::endl;
        return false;
    }

    BaseNPC* npc = NPCManager::NPCs[targetID];
    if (npc->type != EntityType::MOB) {
        std::cout << "[WARN] doHeal: NPC is not a mob" << std::endl;
        return false;
    }

    Mob* targetMob = (Mob*)npc;

    int healedAmount = amount * targetMob->maxHealth / 1000;
    targetMob->appearanceData.iHP += healedAmount;
    if (targetMob->appearanceData.iHP > targetMob->maxHealth)
        targetMob->appearanceData.iHP = targetMob->maxHealth;

    respdata[i].eCT = 4;
    respdata[i].iID = targetMob->appearanceData.iNPC_ID;
    respdata[i].iHP = targetMob->appearanceData.iHP;
    respdata[i].iHealHP = healedAmount;

    return true;
}

bool doReturnHeal(Mob *mob, sSkillResult_Heal_HP *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    int healedAmount = amount * mob->maxHealth / 1000;
    mob->appearanceData.iHP += healedAmount;
    if (mob->appearanceData.iHP > mob->maxHealth)
        mob->appearanceData.iHP = mob->maxHealth;

    respdata[i].eCT = 4;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;
    respdata[i].iHealHP = healedAmount;

    return true;
}

bool doDamage(Mob *mob, sSkillResult_Damage *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doDamage: player ID not found" << std::endl;
        return false;
    }

    int damage = amount * PC_MAXHEALTH((int)mob->data["m_iNpcLevel"]) / 1500;

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE)
        damage = 0;

    respdata[i].eCT = 1;
    respdata[i].iDamage = damage;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP -= damage;

    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        if (!MobAI::aggroCheck(mob, getTime())) {
            MobAI::clearDebuff(mob);
            if (mob->groupLeader != 0)
                MobAI::groupRetreat(mob);
        }
    }

    return true;
}

bool doLeech(Mob *mob, sSkillResult_Heal_HP *healdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    // this sanity check is VERY important
    if (i != 0) {
        std::cout << "[WARN] Mob attempted to leech more than one player!" << std::endl;
        return false;
    }

    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doLeech: player ID not found" << std::endl;
        return false;
    }

    sSkillResult_Damage *damagedata = (sSkillResult_Damage*)(((uint8_t*)healdata) + sizeof(sSkillResult_Heal_HP));

    int healedAmount = amount * PC_MAXHEALTH(plr->level) / 1000;

    mob->appearanceData.iHP += healedAmount;
    if (mob->appearanceData.iHP > mob->maxHealth)
        mob->appearanceData.iHP = mob->maxHealth;

    healdata->eCT = 4;
    healdata->iID = mob->appearanceData.iNPC_ID;
    healdata->iHP = mob->appearanceData.iHP;
    healdata->iHealHP = healedAmount;

    int damage = healedAmount;

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE)
        damage = 0;

    damagedata->eCT = 1;
    damagedata->iDamage = damage;
    damagedata->iID = plr->iID;
    damagedata->iHP = plr->HP -= damage;

    if (plr->HP <= 0) {
        mob->target = nullptr;
        mob->state = MobState::RETREAT;
        if (!MobAI::aggroCheck(mob, getTime())) {
            MobAI::clearDebuff(mob);
            if (mob->groupLeader != 0)
                MobAI::groupRetreat(mob);
        }
    }

    return true;
}

bool doBatteryDrain(Mob *mob, sSkillResult_BatteryDrain *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doBatteryDrain: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;

    if (plr->iConditionBitFlag & CSB_BIT_PROTECT_BATTERY) {
        respdata[i].bProtected = 1;
        respdata[i].iDrainW = 0;
        respdata[i].iDrainN = 0;
    } else {
        respdata[i].bProtected = 0;
        respdata[i].iDrainW = amount * (18 + (int)mob->data["m_iNpcLevel"]) / 36;
        respdata[i].iDrainN = amount * (18 + (int)mob->data["m_iNpcLevel"]) / 36;
    }

    respdata[i].iBatteryW = plr->batteryW -= (respdata[i].iDrainW < plr->batteryW) ? respdata[i].iDrainW : plr->batteryW;
    respdata[i].iBatteryN = plr->batteryN -= (respdata[i].iDrainN < plr->batteryN) ? respdata[i].iDrainN : plr->batteryN;
    respdata[i].iStamina = plr->Nanos[plr->activeNano].iStamina;
    respdata[i].iConditionBitFlag = plr->iConditionBitFlag;

    return true;
}

bool doBuff(Mob *mob, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    respdata[i].eCT = 4;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    mob->appearanceData.iConditionBitFlag |= bitFlag;
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag;

    return true;
}

template<class sPAYLOAD,
         bool (*work)(Mob*,sPAYLOAD*,int,int32_t,int32_t,int16_t,int16_t,int16_t)>
void mobPower(Mob *mob, std::vector<int> targetData,
                int16_t skillID, int16_t duration, int16_t amount, 
                int16_t skillType, int32_t bitFlag, int16_t timeBuffID) {
    size_t resplen;
    // special case since leech is atypically encoded
    if (skillType == EST_BLOODSUCKING)
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Heal_HP) + sizeof(sSkillResult_Damage);
    else
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + targetData[0] * sizeof(sPAYLOAD);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NPC_SKILL_HIT), targetData[0], sizeof(sPAYLOAD))) {
        std::cout << "[WARN] bad sP_FE2CL_NPC_SKILL_HIT packet size" << std::endl;
        return;
    }

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_SKILL_HIT *resp = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;
    sPAYLOAD *respdata = (sPAYLOAD*)(respbuf+sizeof(sP_FE2CL_NPC_SKILL_HIT));

    resp->iNPC_ID = mob->appearanceData.iNPC_ID;
    resp->iSkillID = skillID;
    resp->iValue1 = mob->hitX;
    resp->iValue2 = mob->hitY;
    resp->iValue3 = mob->hitZ;
    resp->eST = skillType;
    resp->iTargetCnt = targetData[0];

    for (int i = 0; i < targetData[0]; i++)
        if (!work(mob, respdata, i, targetData[i+1], bitFlag, timeBuffID, duration, amount))
            return;

    NPCManager::sendToViewable(mob, (void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);
}

// nano power dispatch table
std::vector<MobPower> MobPowers = {
    MobPower(EST_STUN,             CSB_BIT_STUN,              ECSB_STUN,              mobPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    MobPower(EST_HEAL_HP,          CSB_BIT_NONE,              ECSB_NONE,              mobPower<sSkillResult_Heal_HP,                  doHeal>),
    MobPower(EST_RETURNHOMEHEAL,   CSB_BIT_NONE,              ECSB_NONE,              mobPower<sSkillResult_Heal_HP,            doReturnHeal>),
    MobPower(EST_SNARE,            CSB_BIT_DN_MOVE_SPEED,     ECSB_DN_MOVE_SPEED,     mobPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    MobPower(EST_DAMAGE,           CSB_BIT_NONE,              ECSB_NONE,              mobPower<sSkillResult_Damage,                 doDamage>),
    MobPower(EST_BATTERYDRAIN,     CSB_BIT_NONE,              ECSB_NONE,              mobPower<sSkillResult_BatteryDrain,     doBatteryDrain>),
    MobPower(EST_SLEEP,            CSB_BIT_MEZ,               ECSB_MEZ,               mobPower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    MobPower(EST_BLOODSUCKING,     CSB_BIT_NONE,              ECSB_NONE,              mobPower<sSkillResult_Heal_HP,                 doLeech>),
    MobPower(EST_FREEDOM,          CSB_BIT_FREEDOM,           ECSB_FREEDOM,           mobPower<sSkillResult_Buff,                     doBuff>)
};

}; // namespace
#pragma endregion
