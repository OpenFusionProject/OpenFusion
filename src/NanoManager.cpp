#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "GroupManager.hpp"

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

    // Cmd: /nano <nanoID>
    sP_CL2FE_REQ_PC_GIVE_NANO* nano = (sP_CL2FE_REQ_PC_GIVE_NANO*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

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

    summonNano(sock, pkt->iNanoSlotNum);

    // Send to client
    DEBUGLOG(
        std::cout << PlayerManager::getPlayerName(plr) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
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
    if (otherPlr == nullptr)
        return;

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

    sock->sendPacket((void*)&response, P_FE2CL_REP_CHARGE_NANO_STAMINA, sizeof(sP_FE2CL_REP_CHARGE_NANO_STAMINA));
    // now update serverside
    player->batteryN -= difference;
    player->Nanos[nano.iID].iStamina += difference;

}

#pragma region Helper methods
void NanoManager::addNano(CNSocket* sock, int16_t nanoID, int16_t slot, bool spendfm) {
    if (nanoID >= NANO_COUNT)
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
        MissionManager::updateFusionMatter(sock, -(int)MissionManager::AvatarGrowth[plr->level-1]["m_iReqBlob_NanoCreate"]);
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

void NanoManager::summonNano(CNSocket *sock, int slot, bool silent) {
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
        sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));

    // Send to other players, these players can't handle silent nano deaths so this packet needs to be sent.
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);
    pkt1.iPC_ID = plr->iID;
    pkt1.Nano = plr->Nanos[nanoID];
    PlayerManager::sendToViewable(sock, (void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));
}

void NanoManager::setNanoSkill(CNSocket* sock, sP_CL2FE_REQ_NANO_TUNE* skill) {
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
            sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));
            return; // stop execution, don't run consumption logic
        }
    }

#ifndef ACADEMY
    if (plr->fusionmatter < (int)MissionManager::AvatarGrowth[plr->level]["m_iReqBlob_NanoTune"]) // sanity check
        return;
#endif

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
    if (nanoID >= NANO_COUNT)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    sNano nano = plr->Nanos[nanoID];

    // 0 is reset
    nano.iSkillID = 0;
    plr->Nanos[nanoID] = nano;
}

void NanoManager::nanoUnbuff(CNSocket* sock, std::vector<int> targetData, int32_t bitFlag, int16_t timeBuffID, int16_t amount, bool groupPower) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->iSelfConditionBitFlag &= ~bitFlag;
    int groupFlags = 0;

    if (groupPower) {
        plr->iGroupConditionBitFlag &= ~bitFlag;
        Player *leader = PlayerManager::getPlayerFromID(plr->iIDGroup);
        if (leader != nullptr)
            groupFlags = GroupManager::getGroupFlags(leader);
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

int NanoManager::applyBuff(CNSocket* sock, int skillID, int eTBU, int eTBT, int32_t groupFlags) {
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

// 0=A 1=B 2=C -1=Not found
int NanoManager::nanoStyle(int nanoID) {
    if (nanoID < 1 || nanoID >= (int)NanoTable.size())
        return -1;
    return NanoTable[nanoID].style;
}

/*
 * targetData approach
 * first integer is the count
 * second to fifth integers are IDs, these can be either player iID or mob's iID
 */
std::vector<int> NanoManager::findTargets(Player* plr, int skillID, CNPacketData* data) {
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

bool NanoManager::getNanoBoost(Player* plr) {
    for (int i = 0; i < 3; i++) 
        if (plr->equippedNanos[i] == plr->activeNano)
            if (plr->iConditionBitFlag & (CSB_BIT_STIMPAKSLOT1 << i))
                return true;
    return false;
}
#pragma endregion

#pragma region Nano Powers
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
    respdata[i].bProtected = 1;
    if (mob->skillStyle < 0 && mob->state != MobState::RETREAT) { // only debuff if the enemy is not retreating and not casting corruption
        mob->appearanceData.iConditionBitFlag |= bitFlag;
        mob->unbuffTimes[bitFlag] = getTime() + duration * 100;
        respdata[i].bProtected = 0;
    }
    respdata[i].iConditionBitFlag = mob->appearanceData.iConditionBitFlag;

    return true;
}

bool doBuff(CNSocket *sock, sSkillResult_Buff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    Player *plr = nullptr;

    for (auto& pair : PlayerManager::players) {
        if (pair.second->iID == targetID) {
            plr = pair.second;
            break;
        }
    }

    // player not found
    if (plr == nullptr) {
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

        sock->sendPacket((void*)&pkt, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
    }

    return true;
}

bool doDamageNDebuff(CNSocket *sock, sSkillResult_Damage_N_Debuff *respdata, int i, int32_t targetID, int32_t bitFlag, int16_t timeBuffID, int16_t duration, int16_t amount) {
    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamageNDebuff: mob ID not found" << std::endl;
        return false;
    }

    Mob* mob = MobManager::Mobs[targetID];

    MobManager::hitMob(sock, mob, 0); // just to gain aggro

    respdata[i].eCT = 4;
    respdata[i].iDamage = duration / 10;
    respdata[i].iID = mob->appearanceData.iNPC_ID;
    respdata[i].iHP = mob->appearanceData.iHP;
    respdata[i].bProtected = 1;
    if (mob->skillStyle < 0 && mob->state != MobState::RETREAT) { // only debuff if the enemy is not retreating and not casting corruption
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
    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doDamage: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[targetID];

    Player *plr = PlayerManager::getPlayer(sock);

    int damage = MobManager::hitMob(sock, mob, PC_MAXHEALTH(plr->level) * amount / 2000 + mob->appearanceData.iHP * amount / 2000);

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

    if (MobManager::Mobs.find(targetID) == MobManager::Mobs.end()) {
        // not sure how to best handle this
        std::cout << "[WARN] doLeech: mob ID not found" << std::endl;
        return false;
    }
    Mob* mob = MobManager::Mobs[targetID];

    int damage = MobManager::hitMob(sock, mob, amount * 2);

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
