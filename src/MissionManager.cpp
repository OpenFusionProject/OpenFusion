#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "MissionManager.hpp"
#include "PlayerManager.hpp"
#include "ItemManager.hpp"

std::map<int32_t, Reward*> MissionManager::Rewards;

void MissionManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_START, acceptMission);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TASK_END, completeMission);
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

void MissionManager::completeMission(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TASK_END))
        return; // malformed packet

    sP_CL2FE_REQ_PC_TASK_END* missionData = (sP_CL2FE_REQ_PC_TASK_END*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_TASK_END_SUCC, response);
    Player *plr = PlayerManager::getPlayer(sock);

    response.iTaskNum = missionData->iTaskNum;

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
