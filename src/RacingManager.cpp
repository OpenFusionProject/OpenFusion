#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "RacingManager.hpp"
#include "PlayerManager.hpp"
#include "MissionManager.hpp"
#include "ItemManager.hpp"
#include "Database.hpp"
#include "NPCManager.hpp"

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

	if (NPCManager::NPCs.find(req->iEndEcomID) == NPCManager::NPCs.end())
		return; // finish line agent not found

	int mapNum = MAPNUM(NPCManager::NPCs[req->iEndEcomID]->instanceID);
	if (EPData.find(mapNum) == EPData.end() || EPData[mapNum].EPID == 0)
		return; // IZ not found

	uint64_t now = getTime() / 1000;

	int timeDiff = now - EPRaces[sock].startTime;
	int score = 500 * EPRaces[sock].ringCount - 10 * timeDiff;
	int fm = score * plr->level * (1.0f / 36) * (1.0f / 3);

	// we submit the ranking first...
	Database::RaceRanking postRanking = {};
	postRanking.EPID = EPData[mapNum].EPID;
	postRanking.PlayerID = plr->iID;
	postRanking.RingCount = EPRaces[sock].ringCount;
	postRanking.Score = score;
	postRanking.Time = timeDiff;
	postRanking.Timestamp = now;
	Database::postRaceRanking(postRanking);

	// ...then we get the top ranking, which may or may not be what we just submitted
	Database::RaceRanking topRanking = Database::getTopRaceRanking(EPData[mapNum].EPID);

	INITSTRUCT(sP_FE2CL_REP_EP_RACE_END_SUCC, resp);

	resp.iEPTopRank = 5; // top score is always 5 stars, since we're doing it relative to first place
	resp.iEPTopRingCount = topRanking.RingCount;
	resp.iEPTopScore = topRanking.Score;
	resp.iEPTopTime = topRanking.Time;
	
	resp.iEPRaceMode = EPRaces[sock].mode;
	resp.iEPRank = (int)ceil((5.0 * score) / topRanking.Score); // [1, 5] out of 5, relative to top score
	resp.iEPRingCnt = postRanking.RingCount;
	resp.iEPScore = postRanking.Score;
	resp.iEPRaceTime = postRanking.Time;
	resp.iEPRewardFM = fm;

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

	if (reward.iSlotNum > -1) {
		resp.RewardItem = reward;
		plr->Inven[reward.iSlotNum] = item;
	}
	
	EPRaces.erase(sock);
	sock->sendPacket((void*)&resp, P_FE2CL_REP_EP_RACE_END_SUCC, sizeof(sP_FE2CL_REP_EP_RACE_END_SUCC));
}

