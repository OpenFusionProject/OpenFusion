#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "CombatManager.hpp"

namespace NanoManager {

// active powers
std::set<int> StunPowers = {1, 13, 42, 59, 78, 103};
std::set<int> HealPowers = {2, 7, 12, 38, 53, 61, 82, 92, 98};
std::set<int> RecallPowers = {5, 25, 66, 69, 75, 87};
std::set<int> DrainPowers = {10, 34, 37, 56, 93, 97};
std::set<int> SnarePowers = {17, 18, 27, 41, 43, 47, 90, 96, 106};
std::set<int> DamagePowers = {19, 21, 33, 45, 46, 52, 101, 105, 108};
std::set<int> GroupRevivePowers = {20, 63, 91};
std::set<int> LeechPowers = {24, 51, 89};
std::set<int> SleepPowers = {28, 30, 32, 49, 70, 71, 81, 85, 94};

// passive powers
std::set<int> ScavangePowers = {3, 50, 99};
std::set<int> RunPowers = {4, 8, 62, 68, 73, 86};
std::set<int> BonusPowers = {6, 54, 104};
std::set<int> GuardPowers = {9, 57, 76};
std::set<int> RadarPowers = {11, 67, 95};
std::set<int> AnitdotePowers = {14, 58, 102};
std::set<int> FreedomPowers = {15, 31, 39, 55, 77, 107};
std::set<int> JumpPowers = {16, 35, 44, 60, 88, 100};
std::set<int> SelfRevivePowers = {22, 48, 83};
std::set<int> SneakPowers = {23, 29, 65, 72, 80, 82};
std::set<int> TreasureFinderPowers = {26, 40, 74};

/*
 * The active nano power table is down below activePower<>() and its
 * worker functions so we don't have to have unsightly function declarations.
 */

}; // namespace

void NanoManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO, nanoGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO_SKILL, nanoSkillSetGMHandler);
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
    Player *plr = PlayerManager::getPlayer(sock);
    int16_t nanoId = plr->activeNano;
    int16_t skillId = plr->Nanos[nanoId].iSkillID;

    for (auto& pwr : ActivePowers)
        if (pwr.powers.count(skillId)) // std::set's contains method is C++20 only...
            pwr.handle(sock, data, nanoId, skillId);

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

void NanoManager::nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO_SKILL))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skillGM = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skillGM->iNanoID, skillGM->iTuneID);
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

    int level = nanoId < plr->level ? plr->level : nanoId;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoId;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;
    resp.iPC_Level = level;

    // Update player
    plr->Nanos[nanoId] = resp.Nano;
    plr->level = level;
    plr->HP = 925 + 75 * plr->level;
    plr->iConditionBitFlag = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC, sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC));

    /*
     * iPC_Level in NANO_CREATE_SUCC sets the player's level.
     * Other players must be notified of the change as well. Both P_FE2CL_REP_PC_NANO_CREATE and
     * P_FE2CL_REP_PC_CHANGE_LEVEL appear to play the same animation, but only the latter affects
     * the other player's displayed level.
     */

    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_LEVEL, resp2);

    resp2.iPC_ID = plr->iID;
    resp2.iPC_Level = level;

    // Update other players' perception of the player's level
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
}

void NanoManager::summonNano(CNSocket *sock, int slot) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    Player *plr = PlayerManager::getPlayer(sock);

    if (slot > 2 || slot < -1)
        return; // sanity check

    int16_t nanoId = slot == -1 ? -1 : plr->equippedNanos[slot];

    if (nanoId > 36 || nanoId < -1)
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
    
    if (nanoId == -1)
        memset(&pkt1.Nano, 0, sizeof(pkt1.Nano));
    else
        pkt1.Nano = plr->Nanos[nanoId];

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
#pragma endregion

#pragma region Active Powers
namespace NanoManager {

bool doDebuff(CNSocket *sock, int32_t *pktdata, sSkillResult_Damage_N_Debuff *respdata, int i, int16_t iCBFlag, int32_t amount) {
    if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
        return false;
    }

    BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
    
    mob.appearanceData.iHP -= amount;

    if (mob.appearanceData.iHP <= 0)
        CombatManager::giveReward(sock);
    
    respdata[i].eCT = 4;
    respdata[i].iDamage = amount;
    respdata[i].iID = mob.appearanceData.iNPC_ID;
    respdata[i].iHP = mob.appearanceData.iHP;
    respdata[i].iConditionBitFlag = iCBFlag;

    std::cout << (int)mob.appearanceData.iNPC_ID << " was debuffed" << std::endl;

    return true;
}

bool doHeal(CNSocket *sock, int32_t *pktdata, sSkillResult_Heal_HP *respdata, int i, int16_t iCBFlag, int32_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second.plr->iID == pktdata[i]) {
            plr = pair.second.plr;
            break;
        }
    }

    if (plr == nullptr)
        return false;

    if (plr->HP + amount > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);
    else
        plr->HP += amount;

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP;
    respdata[i].iHealHP = amount;
    
    std::cout << (int)plr->iID << " was healed" << std::endl;

    return true;
}

bool doDamage(CNSocket *sock, int32_t *pktdata, sSkillResult_Damage *respdata, int i, int16_t iCBFlag, int32_t amount) {
    if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
        return false;
    }
    BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
    
    mob.appearanceData.iHP -= amount;

    if (mob.appearanceData.iHP <= 0)
        CombatManager::giveReward(sock);
    
    respdata[i].eCT = 4;
    respdata[i].iDamage = amount;
    respdata[i].iID = mob.appearanceData.iNPC_ID;
    respdata[i].iHP = mob.appearanceData.iHP;

    std::cout << (int)mob.appearanceData.iNPC_ID << " was damaged" << std::endl;

    return true;
}

/*
 * NOTE: Leech is specially encoded.
 *
 * It manages to fit inside the activePower<>() mold with only a slight hack,
 * but it really is it's own thing. There is a hard assumption that players
 * will only every leech a single mob, and the sanity check that enforces that
 * assumption is critical.
 */
bool doLeech(CNSocket *sock, int32_t *pktdata, sSkillResult_Heal_HP *healdata, int i, int16_t iCBFlag, int32_t amount) {
    // this sanity check is VERY important
    if (i != 0) {
        std::cout << "[WARN] Player attempted to leech more than one mob!" << std::endl;
        return false;
    }

    sSkillResult_Damage *damagedata = (sSkillResult_Damage*)(((uint8_t*)healdata) + sizeof(sSkillResult_Heal_HP));
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr->HP + amount > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);
    else
        plr->HP += amount;

    healdata->eCT = 1;
    healdata->iID = plr->iID;
    healdata->iHP = plr->HP;
    healdata->iHealHP = amount;

    if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doLeech: mob ID not found" << std::endl;
        return false;
    }
    BaseNPC& mob = NPCManager::NPCs[pktdata[i]];
    
    mob.appearanceData.iHP -= amount;

    if (mob.appearanceData.iHP <= 0)
        CombatManager::giveReward(sock);
    
    damagedata->eCT = 4;
    damagedata->iDamage = amount;
    damagedata->iID = mob.appearanceData.iNPC_ID;
    damagedata->iHP = mob.appearanceData.iHP;

    std::cout << (int)mob.appearanceData.iNPC_ID << " was leeched" << std::endl;

    return true;
}

template<class sPAYLOAD, void *_work, bool isLeech=false>
void activePower(CNSocket *sock, CNPacketData *data,
                 int16_t nanoId, int16_t skillId, SkillType eSkillType,
                 int32_t iCBFlag, int32_t amount) {

    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

    // validate request check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), pkt->iTargetCnt, sizeof(sPAYLOAD))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    size_t resplen;

    // special case since leech is atypically encoded
    if constexpr (isLeech)
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + sizeof(sSkillResult_Heal_HP) + sizeof(sSkillResult_Damage);
    else
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + pkt->iTargetCnt * sizeof(sPAYLOAD);

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE_SUCC *resp = (sP_FE2CL_NANO_SKILL_USE_SUCC*)respbuf;
    sPAYLOAD *respdata = (sPAYLOAD*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));
        
    Player *plr = PlayerManager::getPlayer(sock);
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = (int32_t)eSkillType;
    resp->iTargetCnt = pkt->iTargetCnt;

    // cast the void* to the appropriate function pointer type
    bool (*work)(CNSocket *sock, int32_t *pktdata, sPAYLOAD *respdata, int i, int16_t iCBFlag, int32_t amount) = _work;

    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (!work(sock, pktdata, respdata, i, iCBFlag, amount))
            return;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

// active nano power dispatch table
std::vector<ActivePower> ActivePowers = {
    ActivePower(StunPowers, activePower<sSkillResult_Damage_N_Debuff,  (void*)doDebuff>,         SkillType::STUN, 0x200, 0),
    ActivePower(HealPowers, activePower<sSkillResult_Heal_HP,          (void*)doHeal>,           SkillType::HEAL, 0, 333),
    // TODO: Recall
    ActivePower(DrainPowers, activePower<sSkillResult_Damage_N_Debuff, (void*)doDebuff>,         SkillType::DRAIN, 0x40000, 0),
    ActivePower(SnarePowers, activePower<sSkillResult_Damage_N_Debuff, (void*)doDebuff>,         SkillType::SNARE, 0x80, 0),
    ActivePower(DamagePowers, activePower<sSkillResult_Damage,         (void*)doDamage>,         SkillType::DAMAGE, 0, 133),
    // TODO: GroupRevive
    ActivePower(LeechPowers, activePower<sSkillResult_Heal_HP,         (void*)doLeech, true>,    SkillType::LEECH, 0, 133),
    ActivePower(SleepPowers, activePower<sSkillResult_Damage_N_Debuff, (void*)doDebuff>,         SkillType::SLEEP, 0x400, 0),
};

}; // namespace
#pragma endregion

#pragma region Passive Powers
void NanoManager::nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), 1, sizeof(sSkillResult_Buff))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }
    
    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + sizeof(sSkillResult_Buff);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
        
    memset(respbuf, 0, resplen);
        
    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));
        
    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = 150;
    resp->eST = eSkillType;
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

#pragma endregion
