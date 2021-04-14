#include "servers/CNShardServer.hpp"
#include "Nanos.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "Combat.hpp"
#include "Missions.hpp"
#include "Groups.hpp"
#include "Abilities.hpp"

#include <cmath>

using namespace Nanos;

std::map<int32_t, NanoData> Nanos::NanoTable;
std::map<int32_t, NanoTuning> Nanos::NanoTunings;

#pragma region Helper methods
void Nanos::addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm) {
    if (nanoID <= 0 || nanoID >= NANO_COUNT)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    int level = plr->level;

#ifndef ACADEMY
    level = nanoID < plr->level ? plr->level : nanoID;

    /*
     * Spend the necessary Fusion Matter.
     * Note the use of the not-yet-incremented plr->level as opposed to level.
     * Doing it the other way always leaves the FM at 0. Jade totally called it.
     */
    plr->level = level;

    if (spendfm)
        Missions::updateFusionMatter(sock, -(int)Missions::AvatarGrowth[plr->level-1]["m_iReqBlob_NanoCreate"]);
#endif

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoID;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;
    resp.iPC_Level = level;
    resp.iPC_FusionMatter = plr->fusionmatter;

    if (plr->activeNano > 0 && plr->activeNano == nanoID)
        summonNano(sock, -1); // just unsummon the nano to prevent infinite buffs

    // Update player
    plr->Nanos[nanoID] = resp.Nano;

    sock->sendPacket(resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC);

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
    PlayerManager::sendToViewable(sock, resp2, P_FE2CL_REP_PC_CHANGE_LEVEL);
}

void Nanos::summonNano(CNSocket *sock, int slot, bool silent) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    Player *plr = PlayerManager::getPlayer(sock);

    if (slot > 2 || slot < -1)
        return; // sanity check

    int16_t nanoID = slot == -1 ? 0 : plr->equippedNanos[slot];

    if (slot != -1 && plr->Nanos[nanoID].iSkillID == 0)
        return; // prevent powerless nanos from summoning

    plr->nanoDrainRate = 0;
    int16_t skillID = plr->Nanos[plr->activeNano].iSkillID;

    // passive nano unbuffing
    if (SkillTable[skillID].drainType == 2) {
        std::vector<int> targetData = findTargets(plr, skillID);

        for (auto& pwr : NanoPowers)
            if (pwr.skillType == SkillTable[skillID].skillType)
                nanoUnbuff(sock, targetData, pwr.bitFlag, pwr.timeBuffID, 0,(SkillTable[skillID].targetType == 3));
    }

    if (nanoID >= NANO_COUNT || nanoID < 0)
        return; // sanity check

    plr->activeNano = nanoID;
    skillID = plr->Nanos[nanoID].iSkillID;

    // passive nano buffing
    if (SkillTable[skillID].drainType == 2) {
        std::vector<int> targetData = findTargets(plr, skillID);

        int boost = 0;
        if (getNanoBoost(plr))
            boost = 1;

        for (auto& pwr : NanoPowers) {
            if (pwr.skillType == SkillTable[skillID].skillType) {
                resp.eCSTB___Add = 1; // the part that makes nano go ZOOMAZOOM
                plr->nanoDrainRate = SkillTable[skillID].batteryUse[boost*3];

                pwr.handle(sock, targetData, nanoID, skillID, 0, SkillTable[skillID].powerIntensity[boost]);
            }
        }
    }

    if (!silent) // silent nano death but only for the summoning player
        sock->sendPacket(resp, P_FE2CL_REP_NANO_ACTIVE_SUCC);

    // Send to other players, these players can't handle silent nano deaths so this packet needs to be sent.
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);
    pkt1.iPC_ID = plr->iID;
    pkt1.Nano = plr->Nanos[nanoID];
    PlayerManager::sendToViewable(sock, pkt1, P_FE2CL_NANO_ACTIVE);
}

static void setNanoSkill(CNSocket* sock, sP_CL2FE_REQ_NANO_TUNE* skill) {
    if (skill->iNanoID >= NANO_COUNT)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

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
            sock->sendPacket(resp, P_FE2CL_REP_NANO_TUNE_SUCC);
            return; // stop execution, don't run consumption logic
        }
    }

#ifndef ACADEMY
    if (plr->fusionmatter < (int)Missions::AvatarGrowth[plr->level]["m_iReqBlob_NanoTune"]) // sanity check
        return;
#endif

    plr->fusionmatter -= (int)Missions::AvatarGrowth[plr->level]["m_iReqBlob_NanoTune"];

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

    sock->sendPacket(resp, P_FE2CL_REP_NANO_TUNE_SUCC);

    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " set skill id " << skill->iTuneID << " for nano: " << skill->iNanoID << std::endl;
    )
}

// 0=A 1=B 2=C -1=Not found
int Nanos::nanoStyle(int nanoID) {
    if (nanoID < 1 || nanoID >= (int)NanoTable.size())
        return -1;
    return NanoTable[nanoID].style;
}

bool Nanos::getNanoBoost(Player* plr) {
    for (int i = 0; i < 3; i++) 
        if (plr->equippedNanos[i] == plr->activeNano)
            if (plr->iConditionBitFlag & (CSB_BIT_STIMPAKSLOT1 << i))
                return true;
    return false;
}
#pragma endregion

static void nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
    auto nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_EQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity checks
    if (nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
        return;

    resp.iNanoID = nano->iNanoID;
    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // Update player
    plr->equippedNanos[nano->iNanoSlotNum] = nano->iNanoID;

    // Unbuff gumballs
    int value1 = CSB_BIT_STIMPAKSLOT1 << nano->iNanoSlotNum;
    if (plr->iConditionBitFlag & value1) {
        int value2 = ECSB_STIMPAKSLOT1 + nano->iNanoSlotNum;
        INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
        pkt.eCSTB = value2; // eCharStatusTimeBuffID
        pkt.eTBU = 2; // eTimeBuffUpdate
        pkt.eTBT = 1; // eTimeBuffType 1 means nano
        pkt.iConditionBitFlag = plr->iConditionBitFlag &= ~value1;
        sock->sendPacket(pkt, P_FE2CL_PC_BUFF_UPDATE);
    }

    // unsummon nano if replaced
    if (plr->activeNano == plr->equippedNanos[nano->iNanoSlotNum])
        summonNano(sock, -1);

    sock->sendPacket(resp, P_FE2CL_REP_NANO_EQUIP_SUCC);
}

static void nanoUnEquipHandler(CNSocket* sock, CNPacketData* data) {
    auto nano = (sP_CL2FE_REQ_NANO_UNEQUIP*)data->buf;
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

    sock->sendPacket(resp, P_FE2CL_REP_NANO_UNEQUIP_SUCC);
}

static void nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
    auto pkt = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    summonNano(sock, pkt->iNanoSlotNum);

    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

static void nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    Player *plr = PlayerManager::getPlayer(sock);

    int16_t nanoID = plr->activeNano;
    int16_t skillID = plr->Nanos[nanoID].iSkillID;

    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " requested to summon nano skill " << std::endl;
    )

    std::vector<int> targetData = findTargets(plr, skillID, data);

    int boost = 0;
    if (getNanoBoost(plr))
        boost = 1;

    plr->Nanos[plr->activeNano].iStamina -= SkillTable[skillID].batteryUse[boost*3];
    if (plr->Nanos[plr->activeNano].iStamina < 0)
        plr->Nanos[plr->activeNano].iStamina = 0;

    for (auto& pwr : NanoPowers)
        if (pwr.skillType == SkillTable[skillID].skillType)
            pwr.handle(sock, targetData, nanoID, skillID, SkillTable[skillID].durationTime[boost], SkillTable[skillID].powerIntensity[boost]);

    if (plr->Nanos[plr->activeNano].iStamina < 0)
        summonNano(sock, -1);
}

static void nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
    auto skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skill);
}

static void nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data) {
    auto skillGM = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skillGM);
}

static void nanoRecallRegisterHandler(CNSocket* sock, CNPacketData* data) {
    auto recallData = (sP_CL2FE_REQ_REGIST_RXCOM*)data->buf;

    if (NPCManager::NPCs.find(recallData->iNPCID) == NPCManager::NPCs.end())
        return;

    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC *npc = NPCManager::NPCs[recallData->iNPCID];

    INITSTRUCT(sP_FE2CL_REP_REGIST_RXCOM, response);
    response.iMapNum = plr->recallInstance = (int32_t)npc->instanceID; // Never going to recall into a Fusion Lair
    response.iX = plr->recallX = npc->x;
    response.iY = plr->recallY = npc->y;
    response.iZ = plr->recallZ = npc->z;
    sock->sendPacket(response, P_FE2CL_REP_REGIST_RXCOM);
}

static void nanoRecallHandler(CNSocket* sock, CNPacketData* data) {
    auto recallData = (sP_CL2FE_REQ_WARP_USE_RECALL*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recallData->iGroupMemberID);
    if (otherPlr == nullptr)
        return;

    // ensure the group member is still in the same IZ
    if (otherPlr->instanceID != plr->instanceID)
        return;

    // do not allow hypothetical recall points in lairs to mess with the respawn logic
    if (PLAYERID(plr->instanceID) != 0)
        return;

    if ((int32_t)plr->instanceID == otherPlr->recallInstance)
        PlayerManager::sendPlayerTo(sock, otherPlr->recallX, otherPlr->recallY, otherPlr->recallZ, otherPlr->recallInstance);
    else {
        INITSTRUCT(sP_FE2CL_REP_WARP_USE_RECALL_FAIL, response)
        sock->sendPacket(response, P_FE2CL_REP_WARP_USE_RECALL_FAIL);
    }
}

static void nanoPotionHandler(CNSocket* sock, CNPacketData* data) {
    Player* player = PlayerManager::getPlayer(sock);

    // sanity checks
    if (player->activeNano == -1 || player->batteryN == 0)
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

    sock->sendPacket(response, P_FE2CL_REP_CHARGE_NANO_STAMINA);
    // now update serverside
    player->batteryN -= difference;
    player->Nanos[nano.iID].iStamina += difference;

}

void Nanos::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO_SKILL, nanoSkillSetGMHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_RXCOM, nanoRecallRegisterHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_WARP_USE_RECALL, nanoRecallHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_CHARGE_NANO_STAMINA, nanoPotionHandler);
}
