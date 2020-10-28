#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "GroupManager.hpp"

namespace NanoManager {

// active powers
std::set<int> StunPowers = {1, 13, 42, 59, 78, 103};
std::set<int> HealPowers = {7, 12, 38, 53, 92, 98};
std::set<int> GroupHealPowers = {2, 61, 82};
std::set<int> RecallPowers = {5, 25, 66, 69, 75, 87};
std::set<int> DrainPowers = {10, 34, 37, 56, 93, 97};
std::set<int> SnarePowers = {17, 18, 27, 41, 43, 47, 90, 96, 106};
std::set<int> DamagePowers = {19, 21, 33, 45, 46, 52, 101, 105, 108};
std::set<int> GroupRevivePowers = {20, 63, 91};
std::set<int> LeechPowers = {24, 51, 89};
std::set<int> SleepPowers = {28, 30, 32, 49, 70, 71, 81, 85, 94};

// passive powers
std::set<int> ScavengePowers = {3, 50, 99};
std::set<int> RunPowers = {4, 68, 86};
std::set<int> GroupRunPowers = {8, 62, 73};
std::set<int> BonusPowers = {6, 54, 104};
std::set<int> GuardPowers = {9, 57, 76};
std::set<int> RadarPowers = {11, 67, 95};
std::set<int> AntidotePowers = {14, 58, 102};
std::set<int> FreedomPowers = {31, 39, 107};
std::set<int> GroupFreedomPowers = {15, 55, 77};
std::set<int> JumpPowers = {16, 44, 88};
std::set<int> GroupJumpPowers = {35, 60, 100};
std::set<int> SelfRevivePowers = {22, 48, 84};
std::set<int> SneakPowers = {29, 72, 80};
std::set<int> GroupSneakPowers = {23, 65, 83};
std::set<int> TreasureFinderPowers = {26, 40, 74};

/*
 * The active nano power table is down below activePower<>() and its
 * worker functions so we don't have to have unsightly function declarations.
 */

}; // namespace

std::map<int32_t, NanoData> NanoManager::NanoTable;
std::map<int32_t, NanoTuning> NanoManager::NanoTunings;

void NanoManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO, nanoGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO_SKILL, nanoSkillSetGMHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_WARP_USE_RECALL, nanoRecallHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_CHARGE_NANO_STAMINA, nanoPotionHandler);
}

void NanoManager::nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_EQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_EQUIP* nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_EQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity checks
    if (plr == nullptr || nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
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
    if (plr == nullptr || nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
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

    if (plr == nullptr)
        return;

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

    if (plr == nullptr)
        return;

    summonNano(sock, pkt->iNanoSlotNum);

    // Send to client
    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int16_t nanoId = plr->activeNano;
    int16_t skillId = plr->Nanos[nanoId].iSkillID;

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano skill " << std::endl;
    )

    for (auto& pwr : ActivePowers)
        if (pwr.powers.count(skillId)) // std::set's contains method is C++20 only...
            pwr.handle(sock, data, nanoId, skillId);

    // Group Revive is handled separately (XXX: move into table?)
    if (GroupRevivePowers.find(skillId) == GroupRevivePowers.end())
        return;

    Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (leader == nullptr)
        return;

    for (int i = 0; i < leader->groupCnt; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(leader->groupIDs[i]);

        if (varPlr == nullptr)
            return;

        if (varPlr->HP <= 0)
            revivePlayer(varPlr);
    }
}

void NanoManager::nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_TUNE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skill);
}

void NanoManager::nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO_SKILL))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skillGM = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skillGM);
}

void NanoManager::nanoRecallHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_WARP_USE_RECALL))
        return;

    INITSTRUCT(sP_FE2CL_REP_WARP_USE_RECALL_FAIL, resp);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_WARP_USE_RECALL_FAIL, sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL));
    // stubbed for now
}

void NanoManager::nanoPotionHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_CHARGE_NANO_STAMINA))
        return;

    Player* player = PlayerManager::getPlayer(sock);

    // sanity checks
    if (player == nullptr || player->activeNano == -1 || player->batteryN == 0)
        return;

    sNano nano = player->Nanos[player->activeNano];
    int difference = 150 - nano.iStamina;
    if (player->batteryN < difference)
        difference = player->batteryN;

    if (difference == 0)
        return;

    INITSTRUCT(sP_FE2CL_REP_CHARGE_NANO_STAMINA, response);
    response.iNanoID = nano.iID;
    response.iNanoStamina = nano.iStamina + difference;
    response.iBatteryN = player->batteryN - difference;

    sock->sendPacket((void*)&response, P_FE2CL_REP_CHARGE_NANO_STAMINA, sizeof(sP_FE2CL_REP_CHARGE_NANO_STAMINA));
    // now update serverside
    player->batteryN -= difference;
    player->Nanos[nano.iID].iStamina += difference;

}

#pragma region Helper methods
void NanoManager::addNano(CNSocket* sock, int16_t nanoId, int16_t slot, bool spendfm) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int level = nanoId < plr->level ? plr->level : nanoId;

    /*
     * Spend the necessary Fusion Matter.
     * Note the use of the not-yet-incremented plr->level as opposed to level.
     * Doing it the other way always leaves the FM at 0. Jade totally called it.
     */
    plr->level = level;

    if (spendfm)
        MissionManager::updateFusionMatter(sock, -(int)MissionManager::AvatarGrowth[plr->level-1]["m_iReqBlob_NanoCreate"]);

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoId;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;
    resp.iPC_Level = level;
    resp.iPC_FusionMatter = plr->fusionmatter;

    if (plr->activeNano > 0 && plr->activeNano == nanoId)
        summonNano(sock, -1); // just unsummon the nano to prevent infinite buffs

    // Update player
    plr->Nanos[nanoId] = resp.Nano;

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
    PlayerManager::sendToViewable(sock, (void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
}

void NanoManager::summonNano(CNSocket *sock, int slot) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr || slot > 2 || slot < -1)
        return; // sanity check

    int16_t nanoId = slot == -1 ? -1 : plr->equippedNanos[slot];

    if (nanoId > 36 || nanoId < -1)
        return; // sanity check

    int16_t skillId = 0;

    if (plr->activeNano > 0)
        for (auto& pwr : PassivePowers)
            if (pwr.powers.count(plr->Nanos[plr->activeNano].iSkillID)) { // std::set's contains method is C++20 only...
                nanoUnbuff(sock, pwr.iCBFlag, pwr.eCharStatusTimeBuffID, pwr.iValue, pwr.groupPower);
                plr->passiveNanoOut = false;
            }

    sNano nano = plr->Nanos[nanoId];
    skillId = nano.iSkillID;

    if (slot > -1) {
        plr->activeNano = nanoId;

        for (auto& pwr : PassivePowers)
            if (pwr.powers.count(skillId)) { // std::set's contains method is C++20 only...
                resp.eCSTB___Add = 1;
                nanoBuff(sock, nanoId, skillId, pwr.eSkillType, pwr.iCBFlag, pwr.eCharStatusTimeBuffID, pwr.iValue, pwr.groupPower);
                plr->passiveNanoOut = true;
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

    PlayerManager::sendToViewable(sock, (void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));

    // update player
    plr->activeNano = nanoId;
}

void NanoManager::setNanoSkill(CNSocket* sock, sP_CL2FE_REQ_NANO_TUNE* skill) {
    if (skill->iNanoID > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    if (plr->activeNano > 0 && plr->activeNano == skill->iNanoID)
        summonNano(sock, -1); // just unsummon the nano to prevent infinite buffs

    sNano nano = plr->Nanos[skill->iNanoID];
    nano.iSkillID = skill->iTuneID;
    plr->Nanos[skill->iNanoID] = nano;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_NANO_TUNE_SUCC, resp);
    resp.iNanoID = skill->iNanoID;
    resp.iSkillID = skill->iTuneID;
    resp.iPC_FusionMatter = plr->fusionmatter;
    resp.aItem[9] = plr->Inven[0]; // quick fix to make sure item in slot 0 doesn't get yeeted by default


    // check if there's any garbage in the item slot array (this'll happen when a nano station isn't used)
    for (int i = 0; i < 10; i++) {
        if (skill->aiNeedItemSlotNum[i] < 0 || skill->aiNeedItemSlotNum[i] >= AINVEN_COUNT) {
            sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));
            return; // stop execution, don't run consumption logic
        }
    }

    if (plr->fusionmatter < (int)MissionManager::AvatarGrowth[plr->level]["m_iReqBlob_NanoTune"]) // sanity check
        return;

    plr->fusionmatter -= (int)MissionManager::AvatarGrowth[plr->level]["m_iReqBlob_NanoTune"];

    int reqItemCount = NanoTunings[skill->iTuneID].reqItemCount;
    int reqItemID = NanoTunings[skill->iTuneID].reqItems;
    int i = 0;
    while (reqItemCount > 0 && i < 10) {

        sItemBase& item = plr->Inven[skill->aiNeedItemSlotNum[i]];
        if (item.iType == 7 && item.iID == reqItemID) {
            if (item.iOpt > reqItemCount) {
                item.iOpt -= reqItemCount;
                reqItemCount = 0;
            }
            else {
                reqItemCount -= item.iOpt;
                item.iID = 0;
                item.iType = 0;
                item.iOpt = 0;
            }
        }
        i++; // next slot
    }

    resp.iPC_FusionMatter = plr->fusionmatter; // update fusion matter in packet
    // update items clientside
    for (int i = 0; i < 10; i++) {
        if (skill->aiNeedItemSlotNum[i]) { // non-zero check
            resp.aItem[i] = plr->Inven[skill->aiNeedItemSlotNum[i]];
            resp.aiItemSlotNum[i] = skill->aiNeedItemSlotNum[i];
        }
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " set skill id " << skill->iTuneID << " for nano: " << skill->iNanoID << std::endl;
    )
}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoId) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    sNano nano = plr->Nanos[nanoId];

    // 0 is reset
    nano.iSkillID = 0;
    plr->Nanos[nanoId] = nano;
}
#pragma endregion

#pragma region Active Powers
namespace NanoManager {

bool doDebuff(CNSocket *sock, int32_t *pktdata, sSkillResult_Damage_N_Debuff *respdata, int i, int32_t iCBFlag, int32_t amount) {
    if (MobManager::Mobs.find(pktdata[i]) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
        return false;
    }

    Mob* mob = MobManager::Mobs[pktdata[i]];

    int damage = MobManager::hitMob(sock, mob, 0); // using amount for something else

    respdata[i].eCT = 4;
    respdata[i].iDamage = damage;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag |= iCBFlag;
    mob->unbuffTimes[iCBFlag] = getTime() + amount;
    std::cout << (int)mob->appearanceData.iNPC_ID << " was debuffed" << std::endl;

    return true;
}

bool doBuff(CNSocket *sock, int32_t *pktdata, sSkillResult_Buff *respdata, int i, int32_t iCBFlag, int32_t amount) {
    if (MobManager::Mobs.find(pktdata[i]) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] nanoBuffEnemy: mob ID not found" << std::endl;
        return false;
    }

    Mob* mob = MobManager::Mobs[pktdata[i]];
    MobManager::hitMob(sock, mob, 0);

    respdata[i].eCT = 4;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag |= iCBFlag;
    mob->unbuffTimes[iCBFlag] = getTime() + amount;

    std::cout << (int)mob->appearanceData.iNPC_ID << " was debuffed" << std::endl;

    return true;
}

bool doHeal(CNSocket *sock, int32_t *pktdata, sSkillResult_Heal_HP *respdata, int i, int32_t iCBFlag, int32_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second.plr->iID == pktdata[i]) {
            plr = pair.second.plr;
            break;
        }
    }

    // player not found
    if (plr == nullptr)
        return false;

    int healedAmount = PC_MAXHEALTH(plr->level) * amount / 100;

    plr->HP += healedAmount;

    if (plr->HP > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP;
    respdata[i].iHealHP = healedAmount;

    std::cout << (int)plr->iID << " was healed" << std::endl;

    return true;
}

bool doGroupHeal(CNSocket *sock, int32_t *pktdata, sSkillResult_Heal_HP *respdata, int i, int32_t iCBFlag, int32_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second.plr->iID == pktdata[0]) {
            plr = pair.second.plr;
            break;
        }
    }

    // player not found
    if (plr == nullptr)
        return false;

    Player *leader = PlayerManager::getPlayer(sock);

    // player not found
    if (leader == nullptr)
        return false;

    int healedAmount = PC_MAXHEALTH(plr->level) * amount / 100;

    leader->HP += healedAmount;

    if (leader->HP > PC_MAXHEALTH(leader->level))
        leader->HP = PC_MAXHEALTH(leader->level);

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iHP = plr->HP;
    respdata[i].iHealHP = healedAmount;

    std::cout << (int)plr->iID << " was healed" << std::endl;

    return true;
}

bool doDamage(CNSocket *sock, int32_t *pktdata, sSkillResult_Damage *respdata, int i, int32_t iCBFlag, int32_t amount) {
    if (MobManager::Mobs.find(pktdata[i]) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] nanoDebuffEnemy: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[pktdata[i]];

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return false;

    int damage = MobManager::hitMob(sock, mob, PC_MAXHEALTH(plr->level) * amount / 100);

    respdata[i].eCT = 4;
    respdata[i].iDamage = damage;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;

    std::cout << (int)mob->appearanceData.iNPC_ID << " was damaged" << std::endl;

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
bool doLeech(CNSocket *sock, int32_t *pktdata, sSkillResult_Heal_HP *healdata, int i, int32_t iCBFlag, int32_t amount) {
    // this sanity check is VERY important
    if (i != 0) {
        std::cout << "[WARN] Player attempted to leech more than one mob!" << std::endl;
        return false;
    }

    sSkillResult_Damage *damagedata = (sSkillResult_Damage*)(((uint8_t*)healdata) + sizeof(sSkillResult_Heal_HP));
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return false;

    int healedAmount = PC_MAXHEALTH(plr->level) * amount / 100;

    plr->HP += healedAmount;

    if (plr->HP > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);

    healdata->eCT = 1;
    healdata->iID = plr->iID;
    healdata->iHP = plr->HP;
    healdata->iHealHP = healedAmount;

    if (MobManager::Mobs.find(pktdata[i]) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doLeech: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[pktdata[i]];

    int damage = MobManager::hitMob(sock, mob, PC_MAXHEALTH(plr->level) * amount / 100);

    damagedata->eCT = 4;
    damagedata->iDamage = damage;
    damagedata->iID = mob->appearanceData.iNPC_ID;
    damagedata->iHP = mob->appearanceData.iHP;

    std::cout << (int)mob->appearanceData.iNPC_ID << " was leeched" << std::endl;

    return true;
}

// XXX: Special flags. This is still pretty dirty.
enum {
    NONE,
    LEECH,
    GHEAL
};

template<class sPAYLOAD,
         bool (*work)(CNSocket*,int32_t*,sPAYLOAD*,int,int32_t,int32_t),
         int specialCase=NONE>
void activePower(CNSocket *sock, CNPacketData *data,
                 int16_t nanoId, int16_t skillId, int16_t eSkillType,
                 int32_t iCBFlag, int32_t amount) {

    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

    // validate request check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));

    size_t resplen;

    Player *plr = PlayerManager::getPlayer(sock);
    Player *otherPlr = plr;

    if (plr == nullptr)
        return;

    // special case since leech is atypically encoded
    if constexpr (specialCase == LEECH)
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + sizeof(sSkillResult_Heal_HP) + sizeof(sSkillResult_Damage);
    else if constexpr (specialCase == GHEAL) {
        otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

        if (otherPlr == nullptr)
            return;

        pkt->iTargetCnt = otherPlr->groupCnt;
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + pkt->iTargetCnt * sizeof(sPAYLOAD);
    } else
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + pkt->iTargetCnt * sizeof(sPAYLOAD);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), pkt->iTargetCnt, sizeof(sPAYLOAD))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE_SUCC *resp = (sP_FE2CL_NANO_SKILL_USE_SUCC*)respbuf;
    sPAYLOAD *respdata = (sPAYLOAD*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));

    plr->Nanos[plr->activeNano].iStamina -= 40;
    if (plr->Nanos[plr->activeNano].iStamina < 0)
        plr->Nanos[plr->activeNano].iStamina = 0;

    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
    resp->eST = eSkillType;
    resp->iTargetCnt = pkt->iTargetCnt;

    CNSocket *workSock = sock;

    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if constexpr (specialCase == GHEAL)
            workSock = PlayerManager::getSockFromID(otherPlr->groupIDs[i]);

        if (!work(workSock, pktdata, respdata, i, iCBFlag, amount))
            return;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

// active nano power dispatch table
std::vector<ActivePower> ActivePowers = {
    ActivePower(StunPowers, activePower<sSkillResult_Damage_N_Debuff,  doDebuff>,         EST_STUN, CSB_BIT_STUN,              4000),
    ActivePower(HealPowers, activePower<sSkillResult_Heal_HP,          doHeal>,           EST_HEAL_HP, CSB_BIT_NONE,             35),
    ActivePower(GroupHealPowers, activePower<sSkillResult_Heal_HP,     doGroupHeal, GHEAL>,EST_HEAL_HP, CSB_BIT_NONE,            20),
    // TODO: Recall
    ActivePower(DrainPowers, activePower<sSkillResult_Buff,            doBuff>,        EST_BOUNDINGBALL, CSB_BIT_BOUNDINGBALL, 6000),
    ActivePower(SnarePowers, activePower<sSkillResult_Damage_N_Debuff, doDebuff>,         EST_SNARE, CSB_BIT_DN_MOVE_SPEED,    8000),
    ActivePower(DamagePowers, activePower<sSkillResult_Damage,         doDamage>,         EST_DAMAGE, CSB_BIT_NONE,              12),
    ActivePower(LeechPowers, activePower<sSkillResult_Heal_HP,         doLeech, LEECH>,   EST_BLOODSUCKING, CSB_BIT_NONE,        18),
    ActivePower(SleepPowers, activePower<sSkillResult_Damage_N_Debuff, doDebuff>,         EST_SLEEP, CSB_BIT_MEZ,              8000),
};

}; // namespace
#pragma endregion

#pragma region Passive Powers
void NanoManager::nanoBuff(CNSocket* sock, int16_t nanoId, int skillId, int16_t eSkillType, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue, bool groupPower) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int pktCnt = 1;
    Player *leader = plr;
    plr->iConditionBitFlag |= iCBFlag;

    if (groupPower) {
        plr->iGroupConditionBitFlag |= iCBFlag;

        if (plr->iID != plr->iIDGroup)
            leader = PlayerManager::getPlayerFromID(plr->iIDGroup);

        if (leader == nullptr)
            return;

        pktCnt = leader->groupCnt;
    }

    if (leader == nullptr)
        return;

    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE), pktCnt, sizeof(sSkillResult_Buff))) {
        std::cout << "[WARN] bad sP_FE2CL_NANO_SKILL_USE packet size\n";
        return;
    }

    size_t resplen = sizeof(sP_FE2CL_NANO_SKILL_USE) + pktCnt * sizeof(sSkillResult_Buff);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_NANO_SKILL_USE *resp = (sP_FE2CL_NANO_SKILL_USE*)respbuf;
    sSkillResult_Buff *respdata = (sSkillResult_Buff*)(respbuf+sizeof(sP_FE2CL_NANO_SKILL_USE));

    resp->iPC_ID = plr->iID;
    resp->iSkillID = skillId;
    resp->iNanoID = nanoId;
    resp->iNanoStamina = plr->Nanos[plr->activeNano].iStamina;
    resp->eST = eSkillType;
    resp->iTargetCnt = pktCnt;

    int bitFlag = GroupManager::getGroupFlags(leader);

    for (int i = 0; i < pktCnt; i++) {
        Player* varPlr;
        CNSocket* sockTo;

        if (plr->iID == leader->groupIDs[i]) {
            varPlr = plr;
            sockTo = sock;
        } else {
            varPlr = PlayerManager::getPlayerFromID(leader->groupIDs[i]);
            sockTo = PlayerManager::getSockFromID(leader->groupIDs[i]);
        }

        if (varPlr == nullptr || sockTo == nullptr)
            continue;

        respdata[i].eCT = 1;
        respdata[i].iID = varPlr->iID;
        respdata[i].iConditionBitFlag = iCBFlag;

        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt1);
        pkt1.eCSTB = eCharStatusTimeBuffID; // eCharStatusTimeBuffID
        pkt1.eTBU = 1; // eTimeBuffUpdate
        pkt1.eTBT = 1; // eTimeBuffType 1 means nano
        pkt1.iConditionBitFlag = bitFlag | varPlr->iConditionBitFlag;

        if (iValue > 0)
            pkt1.TimeBuff.iValue = iValue;

        sockTo->sendPacket((void*)&pkt1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NANO_SKILL_USE, resplen);
}

void NanoManager::nanoUnbuff(CNSocket* sock, int32_t iCBFlag, int16_t eCharStatusTimeBuffID, int16_t iValue, bool groupPower) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int pktCnt = 1;
    Player *leader = plr;
    plr->iConditionBitFlag &= ~iCBFlag;

    if (groupPower) {
        plr->iGroupConditionBitFlag &= ~iCBFlag;

        if (plr->iID != plr->iIDGroup)
            leader = PlayerManager::getPlayerFromID(plr->iIDGroup);

        if (leader == nullptr)
            return;

        pktCnt = leader->groupCnt;
    }

    int bitFlag = GroupManager::getGroupFlags(leader);

    for (int i = 0; i < pktCnt; i++) {
        Player* varPlr;
        CNSocket* sockTo;

        if (plr->iID == leader->groupIDs[i]) {
            varPlr = plr;
            sockTo = sock;
        } else {
            varPlr = PlayerManager::getPlayerFromID(leader->groupIDs[i]);
            sockTo = PlayerManager::getSockFromID(leader->groupIDs[i]);
        }

        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp1);
        resp1.eCSTB = eCharStatusTimeBuffID; // eCharStatusTimeBuffID
        resp1.eTBU = 2; // eTimeBuffUpdate
        resp1.eTBT = 1; // eTimeBuffType 1 means nano
        resp1.iConditionBitFlag = bitFlag | varPlr->iConditionBitFlag;

        if (iValue > 0)
            resp1.TimeBuff.iValue = iValue;

        sockTo->sendPacket((void*)&resp1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }
}

// 0=A 1=B 2=C -1=Not found
int NanoManager::nanoStyle(int nanoId) {
    if (nanoId < 1 || nanoId >= (int)NanoTable.size())
        return -1;
    return NanoTable[nanoId].style;
}

namespace NanoManager {

std::vector<PassivePower> PassivePowers = {
    PassivePower(ScavengePowers,       EST_REWARDBLOB,       CSB_BIT_REWARD_BLOB,       ECSB_REWARD_BLOB,       0, false),
    PassivePower(RunPowers,            EST_RUN,              CSB_BIT_UP_MOVE_SPEED,     ECSB_UP_MOVE_SPEED,   200, false),
    PassivePower(GroupRunPowers,       EST_RUN,              CSB_BIT_UP_MOVE_SPEED,     ECSB_UP_MOVE_SPEED,   200,  true),
    PassivePower(BonusPowers,          EST_REWARDCASH,       CSB_BIT_REWARD_CASH,       ECSB_REWARD_CASH,       0, false),
    PassivePower(GuardPowers,          EST_PROTECTBATTERY,   CSB_BIT_PROTECT_BATTERY,   ECSB_PROTECT_BATTERY,   0, false),
    PassivePower(RadarPowers,          EST_MINIMAPENEMY,     CSB_BIT_MINIMAP_ENEMY,     ECSB_MINIMAP_ENEMY,     0, false),
    PassivePower(AntidotePowers,       EST_PROTECTINFECTION, CSB_BIT_PROTECT_INFECTION, ECSB_PROTECT_INFECTION, 0, false),
    PassivePower(FreedomPowers,        EST_FREEDOM,          CSB_BIT_FREEDOM,           ECSB_FREEDOM,           0, false),
    PassivePower(GroupFreedomPowers,   EST_FREEDOM,          CSB_BIT_FREEDOM,           ECSB_FREEDOM,           0,  true),
    PassivePower(JumpPowers,           EST_JUMP,             CSB_BIT_UP_JUMP_HEIGHT,    ECSB_UP_JUMP_HEIGHT,  400, false),
    PassivePower(GroupJumpPowers,      EST_JUMP,             CSB_BIT_UP_JUMP_HEIGHT,    ECSB_UP_JUMP_HEIGHT,  400,  true),
    PassivePower(SelfRevivePowers,     EST_PHOENIX,          CSB_BIT_PHOENIX,           ECSB_PHOENIX,           0, false),
    PassivePower(SneakPowers,          EST_STEALTH,          CSB_BIT_UP_STEALTH,        ECSB_UP_STEALTH,        0, false),
    PassivePower(GroupSneakPowers,     EST_STEALTH,          CSB_BIT_UP_STEALTH,        ECSB_UP_STEALTH,        0,  true),
    PassivePower(TreasureFinderPowers, EST_MINIMAPTRESURE,   CSB_BIT_MINIMAP_TRESURE,   ECSB_MINIMAP_TRESURE,   0, false),
};

}; // namespace

void NanoManager::revivePlayer(Player* plr) {
    CNSocket* sock = PlayerManager::getSockFromID(plr->iID);

    INITSTRUCT(sP_FE2CL_REP_PC_REGEN_SUCC, response);
    INITSTRUCT(sP_FE2CL_PC_REGEN, resp2);

    plr->HP = PC_MAXHEALTH(plr->level);

    // Nanos
    int activeSlot = -1;
    for (int i = 0; i < 3; i++) {
        int nanoID = plr->equippedNanos[i];
        if (plr->activeNano == nanoID) {
            activeSlot = i;
        }
        response.PCRegenData.Nanos[i] = plr->Nanos[nanoID];
    }

    // Response parameters
    response.PCRegenData.iActiveNanoSlotNum = activeSlot;
    response.PCRegenData.iX = plr->x;
    response.PCRegenData.iY = plr->y;
    response.PCRegenData.iZ = plr->z;
    response.PCRegenData.iHP = plr->HP;
    response.iFusionMatter = plr->fusionmatter;
    response.bMoveLocation = 0;
    response.PCRegenData.iMapNum = 0;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_REGEN_SUCC, sizeof(sP_FE2CL_REP_PC_REGEN_SUCC));

    // Update other players
    resp2.PCRegenDataForOtherPC.iPC_ID = plr->iID;
    resp2.PCRegenDataForOtherPC.iX = plr->x;
    resp2.PCRegenDataForOtherPC.iY = plr->y;
    resp2.PCRegenDataForOtherPC.iZ = plr->z;
    resp2.PCRegenDataForOtherPC.iHP = plr->HP;
    resp2.PCRegenDataForOtherPC.iAngle = plr->angle;
    resp2.PCRegenDataForOtherPC.Nano = plr->Nanos[plr->activeNano];

    PlayerManager::sendToViewable(sock, (void*)&resp2, P_FE2CL_PC_REGEN, sizeof(sP_FE2CL_PC_REGEN));
}

void NanoManager::nanoChangeBuff(CNSocket* sock, Player* plr, int32_t cbFrom, int32_t cbTo) {
    bool sentPacket = false;
    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
    resp.eTBU = 3;
    resp.eTBT = 1;
    resp.iConditionBitFlag = cbTo;

    if (!(cbFrom & CSB_BIT_UP_MOVE_SPEED) && (cbTo & CSB_BIT_UP_MOVE_SPEED)) {
        resp.eCSTB = ECSB_UP_MOVE_SPEED;
        resp.eTBU = 1;
        resp.TimeBuff.iValue = 200;
        sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        sentPacket = true;
    } else if ((cbFrom & CSB_BIT_UP_MOVE_SPEED) && !(cbTo & CSB_BIT_UP_MOVE_SPEED)) {
        resp.eCSTB = ECSB_UP_MOVE_SPEED;
        resp.eTBU = 2;
        resp.TimeBuff.iValue = 200;
        sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        sentPacket = true;
    }

    if (!(cbFrom & CSB_BIT_UP_JUMP_HEIGHT) && (cbTo & CSB_BIT_UP_JUMP_HEIGHT)) {
        resp.eCSTB = ECSB_UP_JUMP_HEIGHT;
        resp.eTBU = 1;
        resp.TimeBuff.iValue = 400;
        sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        sentPacket = true;
    } else if ((cbFrom & CSB_BIT_UP_JUMP_HEIGHT) && !(cbTo & CSB_BIT_UP_JUMP_HEIGHT)) {
        resp.eCSTB = ECSB_UP_JUMP_HEIGHT;
        resp.eTBU = 2;
        resp.TimeBuff.iValue = 400;
        sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        sentPacket = true;
    }

    if (!sentPacket)
        sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}
#pragma endregion
