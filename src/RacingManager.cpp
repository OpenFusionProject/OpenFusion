#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "RacingManager.hpp"
#include "PlayerManager.hpp"
#include "MissionManager.hpp"
#include "ItemManager.hpp"

std::map<int32_t, EPInfo> RacingManager::EPData;
std::map<CNSocket*, EPRace> RacingManager::EPRaces;

void RacingManager::init() {

	REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_START, racingStart);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_GET_RING, racingGetPod);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_CANCEL, racingCancel);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_END, racingEnd);
}

void RacingManager::racingStart(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_EP_RACE_START))
		return; // malformed packet

	sP_CL2FE_REQ_EP_RACE_START* req = (sP_CL2FE_REQ_EP_RACE_START*)data->buf;

	// make ongoing race entry
	EPRace race = { 0, req->iEPRaceMode, req->iEPTicketItemSlotNum, getTime() / 1000 };
	EPRaces[sock] = race;

	INITSTRUCT(sP_FE2CL_REP_EP_RACE_START_SUCC, resp);
	resp.iStartTick = 0; // ignored
	resp.iLimitTime = 60 * 20; // TODO: calculate(?) this properly

	sock->sendPacket((void*)&resp, P_FE2CL_REP_EP_RACE_START_SUCC, sizeof(sP_FE2CL_REP_EP_RACE_START_SUCC));
}

void RacingManager::racingGetPod(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_EP_GET_RING))
		return; // malformed packet

	if (EPRaces.find(sock) == EPRaces.end())
		return; // race not found

	sP_CL2FE_REQ_EP_GET_RING* req = (sP_CL2FE_REQ_EP_GET_RING*)data->buf;

	EPRaces[sock].ringCount++;
	
	INITSTRUCT(sP_FE2CL_REP_EP_GET_RING_SUCC, resp);

	resp.iRingLID = req->iRingLID;
	resp.iRingCount_Get = EPRaces[sock].ringCount;

	sock->sendPacket((void*)&resp, P_FE2CL_REP_EP_GET_RING_SUCC, sizeof(sP_FE2CL_REP_EP_GET_RING_SUCC));
}

void RacingManager::racingCancel(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_EP_RACE_CANCEL))
		return; // malformed packet

	if (EPRaces.find(sock) == EPRaces.end())
		return; // race not found

	EPRaces.erase(sock);

	INITSTRUCT(sP_FE2CL_REP_EP_RACE_CANCEL_SUCC, resp);
	sock->sendPacket((void*)&resp, P_FE2CL_REP_EP_RACE_CANCEL_SUCC, sizeof(sP_FE2CL_REP_EP_RACE_CANCEL_SUCC));
}

void RacingManager::racingEnd(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_EP_RACE_END))
		return; // malformed packet

	if (EPRaces.find(sock) == EPRaces.end())
		return; // race not found

	sP_CL2FE_REQ_EP_RACE_END* req = (sP_CL2FE_REQ_EP_RACE_END*)data->buf;
	Player* plr = PlayerManager::getPlayer(sock);

	int timeDiff = getTime() / 1000 - EPRaces[sock].startTime;
	int score = 500 * EPRaces[sock].ringCount - 10 * timeDiff;

	INITSTRUCT(sP_FE2CL_REP_EP_RACE_END_SUCC, resp);
	resp.iEPRaceTime = timeDiff;
	resp.iEPRaceMode = EPRaces[sock].mode;
	resp.iEPRank = 3; // TODO
	resp.iEPScore = score;
	resp.iEPRewardFM = score * plr->level * 1.0f/36 * 1.0f/3; // TODO
	resp.iEPRingCnt = EPRaces[sock].ringCount;

	resp.iEPTopRank = 0; // TODO
	resp.iEPTopRingCount = 0; // TODO
	resp.iEPTopScore = 0; // TODO
	resp.iEPTopTime = 0; // TODO

	MissionManager::updateFusionMatter(sock, resp.iEPRewardFM);

	resp.iFusionMatter = plr->fusionmatter;
	resp.iFatigue = 50;
	resp.iFatigue_Level = 1;

	sItemReward reward;
	reward.iSlotNum = ItemManager::findFreeSlot(plr);
	reward.eIL = 1;
	sItemBase item;
	item.iID = 96;
	item.iType = 0;
	item.iOpt = 1;
	reward.sItem = item;

	if (reward.iSlotNum >= 0) {
		resp.RewardItem = reward;
		plr->Inven[reward.iSlotNum] = item;
	}
	
	sock->sendPacket((void*)&resp, P_FE2CL_REP_EP_RACE_END_SUCC, sizeof(sP_FE2CL_REP_EP_RACE_END_SUCC));
}

