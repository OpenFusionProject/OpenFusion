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
std::map<int32_t, SkillData> NanoManager::SkillTable;

void NanoManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO, nanoGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO_SKILL, nanoSkillSetGMHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_RXCOM, nanoRecallRegisterHandler);
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
    if (plr == nullptr || nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0 || nano->iNanoID > 36)
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
        sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

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

    // Cmd: /nano <nanoID>
    sP_CL2FE_REQ_PC_GIVE_NANO* nano = (sP_CL2FE_REQ_PC_GIVE_NANO*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    // Add nano to player
    addNano(sock, nano->iNanoID, 0);

    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " requested to add nano id: " << nano->iNanoID << std::endl;
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
        std::cout << PlayerManager::getPlayerName(plr) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int16_t nanoID = plr->activeNano;
    int16_t skillID = plr->Nanos[nanoID].iSkillID;

    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " requested to summon nano skill " << std::endl;
    )

    sP_CL2FE_REQ_NANO_SKILL_USE* pkt = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;

    // validate request check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE), pkt->iTargetCnt, sizeof(int32_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_NANO_SKILL_USE packet size" << std::endl;
        return;
    }

    int32_t *pktdata = (int32_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_NANO_SKILL_USE));

    int count = 0;
    INITSTRUCT(TargetData, targetData);

    // this chunky logic chain is responsible for picking all targets of the skill.
    if (SkillTable[skillID].targetType == 1) { // client gives us the targets
        for (int i = 0; i < 4; i++) {
            if (i >= pkt->iTargetCnt)
                targetData.targetID[i] = -1;
            else {
                targetData.targetID[i] = pktdata[i];
                count += 1;
            }
        }
    } else if (SkillTable[skillID].targetType == 2) { // self target only
        targetData.targetID[0] = plr->iID;
        count = 1;

        for (int i = 1; i < 4; i++)
            targetData.targetID[i] = -1;
    } else if (SkillTable[skillID].targetType == 3) { // entire group as target
        Player *otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

        if (otherPlr == nullptr)
            return;

        for (int i = 0; i < 4; i++) {
            if (i >= otherPlr->groupCnt)
                targetData.targetID[i] = -1;
            else {
                targetData.targetID[i] = otherPlr->groupIDs[i];
                count += 1;
            }
        }
    }

    if (count == 0) {
        std::cout << "[WARN] bad power target count" << std::endl;
        count = 1;
    }

    int boost = 0;
    for (int i = 0; i < 3; i++) 
        if (plr->equippedNanos[i] == plr->activeNano)
            if (plr->iConditionBitFlag & (CSB_BIT_STIMPAKSLOT1 << i))
                boost = 1;

    plr->Nanos[plr->activeNano].iStamina -= SkillTable[skillID].batteryUse[boost*3];
    if (plr->Nanos[plr->activeNano].iStamina < 0)
        plr->Nanos[plr->activeNano].iStamina = 0;

    if (count == 4)
        boost = 0;

    for (auto& pwr : ActivePowers)
        if (pwr.skillType == SkillTable[skillID].skillType)
            pwr.handle(sock, targetData, nanoID, skillID, SkillTable[skillID].durationTime[count-1+boost]/count, SkillTable[skillID].powerIntensity[count-1+boost]/count);
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

void NanoManager::nanoRecallRegisterHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REGIST_RXCOM))
        return;

    sP_CL2FE_REQ_REGIST_RXCOM* recallData = (sP_CL2FE_REQ_REGIST_RXCOM*)data->buf;

    if (NPCManager::NPCs.find(recallData->iNPCID) == NPCManager::NPCs.end())
        return;

    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC *npc = NPCManager::NPCs[recallData->iNPCID];

    INITSTRUCT(sP_FE2CL_REP_REGIST_RXCOM, response);
    response.iMapNum = plr->recallInstance = (int32_t)npc->instanceID; // Never going to recall into a Fusion Lair
    response.iX = plr->recallX = npc->appearanceData.iX;
    response.iY = plr->recallY = npc->appearanceData.iY;
    response.iZ = plr->recallZ = npc->appearanceData.iZ;
    sock->sendPacket((void*)&response, P_FE2CL_REP_REGIST_RXCOM, sizeof(sP_FE2CL_REP_REGIST_RXCOM));
}

void NanoManager::nanoRecallHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_WARP_USE_RECALL))
        return;

    sP_CL2FE_REQ_WARP_USE_RECALL* recallData = (sP_CL2FE_REQ_WARP_USE_RECALL*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(recallData->iGroupMemberID);

    if ((int32_t)plr->instanceID == otherPlr->recallInstance)
        PlayerManager::sendPlayerTo(sock, otherPlr->recallX, otherPlr->recallY, otherPlr->recallZ, otherPlr->recallInstance);
    else {
        INITSTRUCT(sP_FE2CL_REP_WARP_USE_RECALL_FAIL, response)
        sock->sendPacket((void*)&response, P_FE2CL_REP_WARP_USE_RECALL_FAIL, sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL));
    }
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
void NanoManager::addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm) {
    if (nanoID > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int level = nanoID < plr->level ? plr->level : nanoID;

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
    resp.Nano.iID = nanoID;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;
    resp.iPC_Level = level;
    resp.iPC_FusionMatter = plr->fusionmatter;

    if (plr->activeNano > 0 && plr->activeNano == nanoID)
        summonNano(sock, -1); // just unsummon the nano to prevent infinite buffs

    // Update player
    plr->Nanos[nanoID] = resp.Nano;

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

    plr->nanoDrainRate = 0;
    int16_t skillID = plr->Nanos[plr->activeNano].iSkillID;
    INITSTRUCT(TargetData, targetData);

    // passive nano unbuffing
    if (SkillTable[skillID].drainType == 2) {
        if (SkillTable[skillID].targetType == 1) {
            std::cout << "[WARN] bad power target type" << std::endl;
            return;
        } else if (SkillTable[skillID].targetType == 2) {
            targetData.targetID[0] = plr->iID;

            for (int i = 1; i < 4; i++)
                targetData.targetID[i] = -1;
        } else if (SkillTable[skillID].targetType == 3) {
            Player *otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            if (otherPlr == nullptr)
                return;

            for (int i = 0; i < 4; i++) {
                if (i >= otherPlr->groupCnt)
                    targetData.targetID[i] = -1;
                else
                    targetData.targetID[i] = otherPlr->groupIDs[i];
            }
        }

        int boost = 0;
        for (int i = 0; i < 3; i++)
            if (plr->equippedNanos[i] == plr->activeNano)
                if (plr->iConditionBitFlag & (CSB_BIT_STIMPAKSLOT1 << i))
                    boost = 1;

        for (auto& pwr : ActivePowers)
            if (pwr.skillType == SkillTable[skillID].skillType)
                nanoUnbuff(sock, targetData, pwr.bitFlag, pwr.timeBuffID, SkillTable[skillID].powerIntensity[boost],(SkillTable[skillID].targetType == 3));
    }

    int16_t nanoID = slot == -1 ? 0 : plr->equippedNanos[slot];

    if (nanoID > 36 || nanoID < 0)
        return; // sanity check

    plr->activeNano = nanoID;
    sNano nano = plr->Nanos[nanoID];
    skillID = nano.iSkillID;

    // passive nano buffing
    if (SkillTable[skillID].drainType == 2) {
        if (SkillTable[skillID].targetType == 1) {
            std::cout << "[WARN] bad power target type" << std::endl;
            return;
        } else if (SkillTable[skillID].targetType == 2) {
            targetData.targetID[0] = plr->iID;

            for (int i = 1; i < 4; i++)
                targetData.targetID[i] = -1;
        } else if (SkillTable[skillID].targetType == 3) {
            Player *otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            if (otherPlr == nullptr)
                return;

            for (int i = 0; i < 4; i++) {
                if (i >= otherPlr->groupCnt)
                    targetData.targetID[i] = -1;
                else
                    targetData.targetID[i] = otherPlr->groupIDs[i];
            }
        }

        int boost = 0;
        for (int i = 0; i < 3; i++)
            if (plr->equippedNanos[i] == plr->activeNano)
                if (plr->iConditionBitFlag & (CSB_BIT_STIMPAKSLOT1 << i))
                    boost = 1;

        for (auto& pwr : ActivePowers) {
            if (pwr.skillType == SkillTable[skillID].skillType) {
                resp.eCSTB___Add = 1; // the part that makes nano go ZOOMAZOOM
                plr->nanoDrainRate = SkillTable[skillID].batteryUse[boost*3];
                pwr.handle(sock, targetData, nanoID, skillID, 0, SkillTable[skillID].powerIntensity[boost]);
            }
        }
    }

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));

    // Send to other players
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);
    pkt1.iPC_ID = plr->iID;
    pkt1.Nano = plr->Nanos[nanoID];
    PlayerManager::sendToViewable(sock, (void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));
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
        std::cout << PlayerManager::getPlayerName(plr) << " set skill id " << skill->iTuneID << " for nano: " << skill->iNanoID << std::endl;
    )
}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoID) {
    if (nanoID > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    sNano nano = plr->Nanos[nanoID];

    // 0 is reset
    nano.iSkillID = 0;
    plr->Nanos[nanoID] = nano;
}

void NanoManager::nanoUnbuff(CNSocket* sock, TargetData targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int count = 0;

    for (int i = 0; i < 4; i++) {
        if (targetData.targetID[i] <= 0) {
            count = i;
            break;
        }
    }

    if (count == 0)
        return;

    plr->iSelfConditionBitFlag &= ~bitFlag;
    int groupFlags = 0;

    if (groupPower) {
        plr->iGroupConditionBitFlag &= ~bitFlag;
        Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);
        if (leader != nullptr)
            groupFlags = GroupManager::getGroupFlags(leader);
    }

    for (int i = 0; i < count; i++) {
        Player* varPlr = PlayerManager::getPlayerFromID(targetData.targetID[i]);
        if (!((groupFlags | varPlr->iSelfConditionBitFlag) & bitFlag)) {
            CNSocket* sockTo = PlayerManager::getSockFromID(targetData.targetID[i]);

            INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
            resp.eCSTB = timeBuffID; // eCharStatusTimeBuffID
            resp.eTBU = 2; // eTimeBuffUpdate
            resp.eTBT = 1; // eTimeBuffType 1 means nano
            resp.iConditionBitFlag = varPlr->iConditionBitFlag = groupFlags | varPlr->iSelfConditionBitFlag;

            if (amount > 0)
                resp.TimeBuff.iValue = amount;

            sockTo->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
        }
    }
}

void NanoManager::applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags) {
    if (SkillTable[skillID].drainType != 2)
        return;

    for (auto& pwr : ActivePowers) {
        if (pwr.skillType == SkillTable[skillID].skillType) {
            Player *plr = PlayerManager::getPlayer(sock);
            if (eTBU == 1 || !((groupFlags | plr->iSelfConditionBitFlag) & pwr.bitFlag)) {
                INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
                resp.eCSTB = pwr.timeBuffID;
                resp.eTBU = eTBU;
                resp.eTBT = eTBT;
                resp.iConditionBitFlag = plr->iConditionBitFlag = groupFlags | plr->iSelfConditionBitFlag;
                resp.TimeBuff.iValue = SkillTable[skillID].powerIntensity[0];

                sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
            }
        }
    }
}

// 0=A 1=B 2=C -1=Not found
int NanoManager::nanoStyle(int nanoID) {
    if (nanoID < 1 || nanoID >= (int)NanoTable.size())
        return -1;
    return NanoTable[nanoID].style;
}
#pragma endregion

#pragma region Active Powers
namespace NanoManager {

bool doDebuff(CNSocket *sock, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        std::cout << "[WARN] doDebuff: mob ID not found" << std::endl;
        return false;
    }

    Mob* mob = MobManager::Mobs[targetID];
    MobManager::hitMob(sock, mob, 0);

    respdata[i].eCT = 4;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag |= bitFlag;
    mob->unbuffTimes[bitFlag] = getTime() + duration * 100;

    std::cout << (int)mob->appearanceData.iNPC_ID << " was debuffed" << std::endl;

    return true;
}

bool doBuff(CNSocket *sock, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == pktdata[i]) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doBuff: player ID not found" << std::endl;
        return false;
    }

    Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (leader == nullptr) {
        std::cout << "[WARN] doBuff: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iConditionBitFlag = bitFlag;

    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt);
    pkt.eCSTB = timeBuffID; // eCharStatusTimeBuffID
    pkt.eTBU = 1; // eTimeBuffUpdate
    pkt.eTBT = 1; // eTimeBuffType 1 means nano
    pkt.iConditionBitFlag = plr->iConditionBitFlag = GroupManager::getGroupFlags(leader) | plr->iSelfConditionBitFlag;

    if (amount > 0)
        pkt.TimeBuff.iValue = amount;

    sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

    return true;
}

bool doDamageNDebuff(CNSocket *sock, sSkillResult_Damage_N_Debuff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamageNDebuff: mob ID not found" << std::endl;
        return false;
    }

    Mob* mob = MobManager::Mobs[targetID];

    int damage = MobManager::hitMob(sock, mob, 0); // using amount for something else

    respdata[i].eCT = 4;
    respdata[i].iDamage = damage;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag |= bitFlag;
    mob->unbuffTimes[bitFlag] = getTime() + duration * 100;
    std::cout << (int)mob->appearanceData.iNPC_ID << " was debuffed" << std::endl;

    return true;
}

bool doHeal(CNSocket *sock, sSkillResult_Heal_HP *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == pktdata[0]) {
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

bool doDamage(CNSocket *sock, sSkillResult_Damage *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamage: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[targetID];

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return false;

    int damage = MobManager::hitMob(sock, mob, PC_MAXHEALTH(plr->level) * amount / 1000);

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
 
bool doLeech(CNSocket *sock, sSkillResult_Heal_HP *healdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    // this sanity check is VERY important
    if (i != 0) {
        std::cout << "[WARN] Player attempted to leech more than one mob!" << std::endl;
        return false;
    }

    sSkillResult_Damage *damagedata = (sSkillResult_Damage*)(((uint8_t*)healdata) + sizeof(sSkillResult_Heal_HP));
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return false;

    int healedAmount = PC_MAXHEALTH(plr->level) * amount / 2000;

    plr->HP += healedAmount;

    if (plr->HP > PC_MAXHEALTH(plr->level))
        plr->HP = PC_MAXHEALTH(plr->level);

    healdata->eCT = 1;
    healdata->iID = plr->iID;
    healdata->iHP = plr->HP;
    healdata->iHealHP = healedAmount;

    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doLeech: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[targetID];

    int damage = MobManager::hitMob(sock, mob, PC_MAXHEALTH(plr->level) * amount / 1000);

    damagedata->eCT = 4;
    damagedata->iDamage = damage;
    damagedata->iID = mob->appearanceData.iNPC_ID;
    damagedata->iHP = mob->appearanceData.iHP;

    std::cout << (int)mob->appearanceData.iNPC_ID << " was leeched" << std::endl;

    return true;
}

bool doResurrect(CNSocket *sock, sSkillResult_Resurrect *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second.plr->iID == targetID) {
            plr = pair.second.plr;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doResurrect: player ID not found" << std::endl;
        return false;
    }

    Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);

    if (leader == nullptr) {
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
        if (pair.second.plr->iID == targetID) {
            plr = pair.second.plr;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
        std::cout << "[WARN] doMove: player ID not found" << std::endl;
        return false;
    }

    respdata[i].eCT = 1;
    respdata[i].iID = plr->iID;
    respdata[i].iMapNum = plr->recallInstance;
    respdata[i].iMoveX = plr->recallX;
    respdata[i].iMoveY = plr->recallY;
    respdata[i].iMoveZ = plr->recallZ;

    return true;
}

template<class sPAYLOAD,
         bool (*work)(CNSocket*,sPAYLOAD*,int,int32_t,int32_t,int16_t,int16_t,int16_t)>
void activePower(CNSocket *sock, TargetData targetData, 
                int16_t nanoID, int16_t skillID, int16_t duration, int16_t amount, 
                int16_t skillType, int32_t bitFlag, int16_t timeBuffID) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    int count = 0;

    for (int i = 0; i < 4; i++)
        if (targetData.targetID[i] <= 0) {
            count = i;
            break;
        }

    if (count == 0)
        return;

    if (skillType == EST_RETROROCKET_SELF || skillType == EST_RECALL) // rocket and self recall does not need any trailing structs
        count = 0;

    size_t resplen;
    // special case since leech is atypically encoded
    if (skillType == EST_BLOODSUCKING)
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + sizeof(sSkillResult_Heal_HP) + sizeof(sSkillResult_Damage);
    else
        resplen = sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) + count * sizeof(sPAYLOAD);

    // validate response packet
    if (!validOutVarPacket(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), count, sizeof(sPAYLOAD))) {
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
    resp->iTargetCnt = count;

    if (SkillTable[skillID].drainType == 2) {
        if (SkillTable[skillID].targetType >= 2)
            plr->iSelfConditionBitFlag |= bitFlag;
        if (SkillTable[skillID].targetType == 3)
            plr->iGroupConditionBitFlag |= bitFlag;
    }

    CNSocket *workSock = sock;

    for (int i = 0; i < count; i++) {
        if (SkillTable[skillID].targetType == 3)
            workSock = PlayerManager::getSockFromID(targetData.targetID[i]);
        if (skillType == EST_RECALL || skillType == EST_RECALL_GROUP)
            targetData.targetID[i] = plr->iID;
        if (!work(workSock, respdata, i, targetData.targetID[i], bitFlag, timeBuffID, duration, amount))
            return;
    }

    sock->sendPacket((void*)&respbuf, P_FE2CL_NANO_SKILL_USE_SUCC, resplen);
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

// active nano power dispatch table
std::vector<ActivePower> ActivePowers = {
    ActivePower(EST_STUN,             CSB_BIT_STUN,              ECSB_STUN,              activePower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    ActivePower(EST_HEAL_HP,          CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Heal_HP,                  doHeal>),
    ActivePower(EST_BOUNDINGBALL,     CSB_BIT_BOUNDINGBALL,      ECSB_BOUNDINGBALL,      activePower<sSkillResult_Buff,                   doDebuff>),
    ActivePower(EST_SNARE,            CSB_BIT_DN_MOVE_SPEED,     ECSB_DN_MOVE_SPEED,     activePower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    ActivePower(EST_DAMAGE,           CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Damage,                 doDamage>),
    ActivePower(EST_BLOODSUCKING,     CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Heal_HP,                 doLeech>),
    ActivePower(EST_SLEEP,            CSB_BIT_MEZ,               ECSB_MEZ,               activePower<sSkillResult_Damage_N_Debuff, doDamageNDebuff>),
    ActivePower(EST_REWARDBLOB,       CSB_BIT_REWARD_BLOB,       ECSB_REWARD_BLOB,       activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_RUN,              CSB_BIT_UP_MOVE_SPEED,     ECSB_UP_MOVE_SPEED,     activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_REWARDCASH,       CSB_BIT_REWARD_CASH,       ECSB_REWARD_CASH,       activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_PROTECTBATTERY,   CSB_BIT_PROTECT_BATTERY,   ECSB_PROTECT_BATTERY,   activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_MINIMAPENEMY,     CSB_BIT_MINIMAP_ENEMY,     ECSB_MINIMAP_ENEMY,     activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_PROTECTINFECTION, CSB_BIT_PROTECT_INFECTION, ECSB_PROTECT_INFECTION, activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_JUMP,             CSB_BIT_UP_JUMP_HEIGHT,    ECSB_UP_JUMP_HEIGHT,    activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_FREEDOM,          CSB_BIT_FREEDOM,           ECSB_FREEDOM,           activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_PHOENIX,          CSB_BIT_PHOENIX,           ECSB_PHOENIX,           activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_STEALTH,          CSB_BIT_UP_STEALTH,        ECSB_UP_STEALTH,        activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_MINIMAPTRESURE,   CSB_BIT_MINIMAP_TRESURE,   ECSB_MINIMAP_TRESURE,   activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_RECALL,           CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Move,                     doMove>),
    ActivePower(EST_RECALL_GROUP,     CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Move,                     doMove>),
    ActivePower(EST_RETROROCKET_SELF, CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Buff,                     doBuff>),
    ActivePower(EST_PHOENIX_GROUP,    CSB_BIT_NONE,              ECSB_NONE,              activePower<sSkillResult_Resurrect,           doResurrect>)
};

}; // namespace
#pragma endregion