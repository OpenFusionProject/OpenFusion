#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "MissionManager.hpp"
#include "PlayerManager.hpp"
#include "ItemManager.hpp"

#include "string.h"

std::map<int32_t, Reward*> MissionManager::Rewards;
std::map<int32_t, TaskData*> MissionManager::Tasks;

void MissionManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_START, taskStart);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_END, taskEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID, setMission);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_STOP, quitMission);
}

void MissionManager::taskStart(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_START))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_START* missionData = (sP_CL2FE_REQ_PC_TASK_START*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_START_SUCC, response);
    Player *plr = PlayerManager::getPlayer(sock);

    if (Tasks.find(missionData->iTaskNum) == Tasks.end()) {
        std::cout << "[WARN] Player submitted unknown task!?" << std::endl;
        // TODO: TASK_FAIL?
        response.iTaskNum = missionData->iTaskNum;
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_START_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC));
        return;
    }

    int i;
    for (i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] == 0) {
            plr->tasks[i] = missionData->iTaskNum;
            break;
        }
    }
    if (i == ACTIVE_MISSION_COUNT - 1 && plr->tasks[i] != missionData->iTaskNum) {
        std::cout << "[WARN] Player has more than 6 active missions!?" << std::endl;
    }

    response.iTaskNum = missionData->iTaskNum;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_START_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC));
}

void MissionManager::taskEnd(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_END))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_END* missionData = (sP_CL2FE_REQ_PC_TASK_END*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_END_SUCC, response);
    Player *plr = PlayerManager::getPlayer(sock);

    response.iTaskNum = missionData->iTaskNum;
    //std::cout << missionData->iTaskNum << std::endl;

    if (Tasks.find(missionData->iTaskNum) == Tasks.end()) {
        std::cout << "[WARN] Player submitted unknown task!?" << std::endl;
        // TODO: TASK_FAIL?
        sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_END_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC));
        return;
    }

    // ugly pointer/reference juggling for the sake of operator overloading...
    TaskData& task = *Tasks[missionData->iTaskNum];

    /*
     * Give (or take away) quest items
     *
     * Some mission tasks give the player a quest item upon completion.
     * This is distinct from quest item mob drops.
     * They can be identified by a counter in the task indicator (ie. 1/1 Gravity Decelerator).
     * The server is responsible for dropping the correct item.
     * Yes, this is pretty stupid.
     *
     * iSUInstancename is the number of items to give. It is usually negative at the end of
     * a mission, so as to clean up it's quest items.
     */
    for (int i = 0; i < 3; i++)
        if (task["m_iSUItem"][i] != 0)
            dropQuestItem(sock, missionData->iTaskNum, task["m_iSUInstancename"][i], task["m_iSUItem"][i], 0);

    // mission rewards
    if (Rewards.find(missionData->iTaskNum) != Rewards.end()) {
        if (giveMissionReward(sock, missionData->iTaskNum) == -1)
            return;
    }

    // update player
    int i;
    for (i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] == missionData->iTaskNum)
            plr->tasks[i] = 0;
    }
    if (i == ACTIVE_MISSION_COUNT - 1 && plr->tasks[i] != 0) {
        std::cout << "[WARN] Player completed non-active mission!?" << std::endl;
    }

    // if it's the last task
    if (task["m_iSUOutgoingTask"] == 0)
    {
        // save completed mission on player
        saveMission(plr, (int)(task["m_iHMissionID"])-1);
    }

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_END_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC));
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
    Player *plr = PlayerManager::getPlayer(sock);

    // update player
    int i;
    for (i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] == missionData->iTaskNum)
            plr->tasks[i] = 0;
    }
    if (i == ACTIVE_MISSION_COUNT - 1 && plr->tasks[i] != 0) {
        std::cout << "[WARN] Player quit non-active mission!?" << std::endl;
    }

    TaskData& task = *Tasks[missionData->iTaskNum];

    // clean up quest items
    for (i = 0; i < 3; i++) {
        if (task["m_iSUItem"][i] == 0 && task["m_iCSUItem"][i] == 0)
            continue;

        /*
         * It's ok to do this only server-side, because the server decides which
         * slot later items will be placed in.
         */
        for (int j = 0; j < AQINVEN_COUNT; j++)
            if (plr->QInven[j].iID == task["m_iSUItem"][i] || plr->QInven[j].iID == task["m_iCSUItem"][i])
                memset(&plr->QInven[j], 0, sizeof(sItemBase));
    }

    response.iTaskNum = missionData->iTaskNum;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_TASK_STOP_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_STOP_SUCC));
}

// TODO: coalesce into ItemManager::findFreeSlot()?
int MissionManager::findQSlot(Player *plr, int id) {
    int i;

    // two passes. we mustn't fail to find an existing stack.
    for (i = 0; i < AQINVEN_COUNT; i++)
        if (plr->QInven[i].iID == id)
            return i;

    // no stack. start a new one.
    for (i = 0; i < AQINVEN_COUNT; i++)
        if (plr->QInven[i].iOpt == 0)
            return i;

    // not found
    return -1;
}

void MissionManager::dropQuestItem(CNSocket *sock, int task, int count, int id, int mobid) {
    const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
    assert(resplen < CN_PACKET_BUFFER_SIZE);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[resplen]; // not a variable length array, don't worry
    sP_FE2CL_REP_REWARD_ITEM *reward = (sP_FE2CL_REP_REWARD_ITEM *)respbuf;
    sItemReward *item = (sItemReward *)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));
    Player *plr = PlayerManager::getPlayer(sock);

    // don't forget to zero the buffer!
    memset(respbuf, 0, resplen);

    // find free quest item slot
    int slot = findQSlot(plr, id);
    if (slot == -1) {
        // this should never happen
        std::cout << "[WARN] Player has no room for quest item!?" << std::endl;
        return;
    }
    if (id != 0)
        std::cout << "new qitem in slot " << slot << std::endl;

    // update player
    if (id != 0) {
        plr->QInven[slot].iType = 8;
        plr->QInven[slot].iID = id;
        plr->QInven[slot].iOpt += count; // stacking
    }

    // fully destory deleted items, for good measure
    if (plr->QInven[slot].iOpt <= 0)
        memset(&plr->QInven[slot], 0, sizeof(sItemBase));

    // preserve stats
    reward->m_iCandy = plr->money;
    reward->m_iFusionMatter = plr->fusionmatter;
    reward->iFatigue = 100; // prevents warning message
    reward->iFatigue_Level = 1;

    reward->iItemCnt = 1; // remember to update resplen if you change this
    reward->iTaskID = task;
    reward->iNPC_TypeID = mobid;

    item->sItem = plr->QInven[slot];
    item->iSlotNum = slot;
    item->eIL = 2;

    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
}

int MissionManager::giveMissionReward(CNSocket *sock, int task) {
    Reward *reward = Rewards[task];
    Player *plr = PlayerManager::getPlayer(sock);

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

            fail.iTaskNum = task;
            fail.iErrorCode = 13; // inventory full

            sock->sendPacket((void*)&fail, P_FE2CL_REP_PC_TASK_END_FAIL, sizeof(sP_FE2CL_REP_PC_TASK_END_FAIL));
            return -1;
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

    sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
    return 0;
}

void MissionManager::mobKilled(CNSocket *sock, int mobid) {
    Player *plr = PlayerManager::getPlayer(sock);
    bool missionmob = false;

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] == 0)
            continue;

        // tasks[] should always have valid IDs
        TaskData& task = *Tasks[plr->tasks[i]];

        for (int j = 0; j < 3; j++) {
            if (task["m_iCSUEnemyID"][j] != mobid)
                continue;

            // acknowledge killing of mission mob...
            if (task["m_iCSUNumToKill"][j] != 0)
                missionmob = true;

            // drop quest item
            if (task["m_iCSUItemNumNeeded"][j] != 0) {
                bool drop = rand() % 100 < task["m_iSTItemDropRate"][j];
                if (drop) {
                    // XXX: are CSUItemID and CSTItemID the same?
                    dropQuestItem(sock, plr->tasks[i], 1, task["m_iCSUItemID"][j], mobid);
                } else {
                    // fail to drop (itemID == 0)
                    dropQuestItem(sock, plr->tasks[i], 1, 0, mobid);
                }
            }
        }
    }

    // ...but only once
    // XXX: is it actually necessary to do it this way?
    if (missionmob) {
        INITSTRUCT(sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC, kill);

        kill.iNPCID = mobid;

        sock->sendPacket((void*)&kill, P_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC, sizeof(sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC));
    }
}

void MissionManager::saveMission(Player* player, int missionId) {
    // Missions are stored in int_64t array
    int row = missionId / 64;
    int column = missionId % 64;
    player->aQuestFlag[row] |= (1ULL << column);
}
