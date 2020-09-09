#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "MissionManager.hpp"
#include "PlayerManager.hpp"
#include "ItemManager.hpp"

std::map<int32_t, Reward*> MissionManager::Rewards;
std::map<int32_t, SUItem*> MissionManager::SUItems;
std::map<int32_t, QuestDropSet*> MissionManager::QuestDropSets;

void MissionManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_START, acceptMission);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_END, completeTask);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID, setMission);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_STOP, quitMission);
}

void MissionManager::acceptMission(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_START))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_START* missionData = (sP_CL2FE_REQ_PC_TASK_START*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_START_SUCC, response);

    response.iTaskNum = missionData->iTaskNum;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_START_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC));
}

void MissionManager::completeTask(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_END))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_END* missionData = (sP_CL2FE_REQ_PC_TASK_END*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_END_SUCC, response);
    Player *plr = PlayerManager::getPlayer(sock);

    response.iTaskNum = missionData->iTaskNum;
    std::cout << missionData->iTaskNum << std::endl;

    /*
     * SUItems
     *
     * Some mission tasks give the player a quest item upon completion.
     * This is distinct from quest item mob drops.
     * They can be identified by a counter in the task indicator (ie. 1/1 Gravity Decelerator).
     * The server is responsible for dropping the correct item.
     * Yes, this is idiotic.
     */
    if (SUItems.find(missionData->iTaskNum) != SUItems.end()) {
        SUItem *suitem = SUItems[missionData->iTaskNum];

        const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
        assert(resplen < CN_PACKET_BUFFER_SIZE);
        // we know it's only one trailing struct, so we can skip full validation

        uint8_t respbuf[resplen]; // not a variable length array, don't worry
        sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
        sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

        // don't forget to zero the buffer!
        memset(respbuf, 0, resplen);

        reward->m_iCandy = plr->money;
        reward->m_iFusionMatter = plr->fusionmatter;
        reward->iFatigue = 100; // prevents warning message
        reward->iFatigue_Level = 1;
        reward->iItemCnt = 1; // remember to update resplen if you change this
        reward->iTaskID = missionData->iTaskNum;
        reward->iNPC_TypeID = 0; // XXX

        item->sItem.iType = 8;
        item->sItem.iID = suitem->itemIds[0]; // XXX XXX XXX
        item->sItem.iOpt = 1; // XXX count needs to be correct
        item->iSlotNum = 0; // XXX
        item->eIL = 2;

        std::cout << "sending item packet\n";
        sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    }

    // mission rewards
    if (Rewards.find(missionData->iTaskNum) == Rewards.end()) {
        // not a mission's final task
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_END_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC));
        return;
    }
    Reward *reward = Rewards[missionData->iTaskNum];

    int nrewards = 0;
    for (int i = 0; i < 4; i++) {
        if (reward->itemIds[i] != 0)
            nrewards++;
    }

    int slots[4];
    for (int i = 0; i < nrewards; i++) {
        slots[i] = ItemManager::findFreeSlot(plr);
        if (slots[i] == -1) {
            std::cout << "Not enough room to complete task" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_TASK_END_FAIL, fail);

            fail.iTaskNum = missionData->iTaskNum;
            fail.iErrorCode = 13; // inventory full

            sock->sendPacket((void*)&fail, P_FE2CL_REP_PC_TASK_END_FAIL, sizeof(sP_FE2CL_REP_PC_TASK_END_FAIL));
            return;
        }
    }

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + nrewards * sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE);
    sP_FE2CL_REP_REWARD_ITEM *resp = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // update player
    plr->money += reward->money;
    plr->fusionmatter += reward->fusionmatter;

    // simple rewards
    resp->m_iCandy = plr->money;
    resp->m_iFusionMatter = plr->fusionmatter;
    resp->iFatigue = 100; // prevents warning message
    resp->iFatigue_Level = 1;
    resp->iItemCnt = nrewards;

    for (int i = 0; i < nrewards; i++) {
        item[i].sItem.iType = reward->itemTypes[i];
        item[i].sItem.iID = reward->itemIds[i];
        item[i].iSlotNum = slots[i];
        item[i].eIL = 1;

        // update player
        plr->Inven[slots[i]] = item->sItem;
    }

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_END_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC));
    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
}

void MissionManager::setMission(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID))
        return; // malformed packet

    sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID* missionData = (sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID, response);

    response.iCurrentMissionID = missionData->iCurrentMissionID;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_SET_CURRENT_MISSION_ID, sizeof(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID));
}

void MissionManager::quitMission(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_STOP))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_STOP* missionData = (sP_CL2FE_REQ_PC_TASK_STOP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_STOP_SUCC, response);

    response.iTaskNum = missionData->iTaskNum;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_STOP_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_STOP_SUCC));
}

//void MissionManager::dropQuestItem(CNSocket *sock, int mobid, int count) {}
