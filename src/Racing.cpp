#include "Racing.hpp"
#include "db/Database.hpp"
#include "servers/CNShardServer.hpp"
#include "NPCManager.hpp"
#include "PlayerManager.hpp"
#include "Missions.hpp"
#include "Items.hpp"

using namespace Racing;

std::map<int32_t, EPInfo> Racing::EPData;
std::map<CNSocket*, EPRace> Racing::EPRaces;
std::map<int32_t, std::pair<std::vector<int>, std::vector<int>>> Racing::EPRewards;

namespace {
    // Helper functions
    bool validateRaceStart(CNSocket* sock, int32_t startEcomID) {
        if (NPCManager::NPCs.find(startEcomID) == NPCManager::NPCs.end())
            return false; // starting line agent not found

        int mapNum = MAPNUM(NPCManager::NPCs[startEcomID]->instanceID);
        return EPData.find(mapNum) != EPData.end() && EPData[mapNum].EPID != 0;
    }

    bool validateRaceEnd(CNSocket* sock, int32_t endEcomID) {
        if (EPRaces.find(sock) == EPRaces.end() || 
            NPCManager::NPCs.find(endEcomID) == NPCManager::NPCs.end())
            return false;

        int mapNum = MAPNUM(NPCManager::NPCs[endEcomID]->instanceID);
        return EPData.find(mapNum) != EPData.end() && EPData[mapNum].EPID != 0;
    }

    int calculateRaceScore(const EPInfo& epInfo, int podsCollected, int timeDiff) {
        int score = std::exp(
            (epInfo.podFactor * podsCollected) / epInfo.maxPods
            - (epInfo.timeFactor * timeDiff) / epInfo.maxTime
            + epInfo.scaleFactor);
        
        return (settings::IZRACESCORECAPPED && score > epInfo.maxScore) 
            ? epInfo.maxScore : score;
    }

    int calculateFMReward(const EPInfo& epInfo, int podsCollected) {
        return (1.0 + std::exp(epInfo.scaleFactor - 1.0) * epInfo.podFactor * podsCollected) 
            / epInfo.maxPods;
    }

    void handleRaceRanking(const EPInfo& epInfo, Player* plr, int podsCollected, 
                          int score, int timeDiff, sP_FE2CL_REP_EP_RACE_END_SUCC& resp) {
        Database::RaceRanking postRanking = {
            .EPID = epInfo.EPID,
            .PlayerID = plr->iID,
            .RingCount = podsCollected,
            .Score = score,
            .Time = timeDiff,
            .Timestamp = getTimestamp()
        };
        Database::postRaceRanking(postRanking);

        Database::RaceRanking topRankingPlayer = Database::getTopRaceRanking(epInfo.EPID, plr->iID);

        std::vector<int>* rankScores = &EPRewards[epInfo.EPID].first;
        std::vector<int>* rankRewards = &EPRewards[epInfo.EPID].second;
        int maxRank = rankScores->size() - 1;

        int topRank = 0;
        while (topRank < maxRank && rankScores->at(topRank) > topRankingPlayer.Score)
            topRank++;

        int currentRank = 0;
        while (currentRank < maxRank && rankScores->at(currentRank) > postRanking.Score)
            currentRank++;

        resp.iEPTopRank = topRank + 1;
        resp.iEPTopRingCount = topRankingPlayer.RingCount;
        resp.iEPTopScore = topRankingPlayer.Score;
        resp.iEPTopTime = topRankingPlayer.Time;
        resp.iEPRank = currentRank + 1;
        resp.iEPRingCnt = postRanking.RingCount;
        resp.iEPScore = postRanking.Score;
        resp.iEPRaceTime = postRanking.Time;

        if (currentRank <= maxRank) {
            sItemReward reward;
            reward.iSlotNum = Items::findFreeSlot(plr);
            if (reward.iSlotNum > -1) {
                reward.eIL = 1;
                sItemBase item = {
                    .iID = rankRewards->at(currentRank),
                    .iType = 9,
                    .iOpt = 1,
                    .iTimeLimit = 0
                };
                reward.sItem = item;
                if (reward.sItem.iID != 0) {
                    resp.RewardItem = reward;
                    plr->Inven[reward.iSlotNum] = item;
                }
            }
        }
    }
}

void Racing::racingStart(CNSocket* sock, CNPacketData* data) {
    auto req = (sP_CL2FE_REQ_EP_RACE_START*)data->buf;
    
    if (!validateRaceStart(sock, req->iStartEcomID))
        return;

    int mapNum = MAPNUM(NPCManager::NPCs[req->iStartEcomID]->instanceID);
    
    EPRaces[sock] = {
        std::set<int32_t>(),
        req->iEPRaceMode,
        req->iEPTicketItemSlotNum,
        getTime() / 1000
    };

    INITSTRUCT(sP_FE2CL_REP_EP_RACE_START_SUCC, resp);
    resp.iStartTick = 0;
    resp.iLimitTime = EPData[mapNum].maxTime;
    sock->sendPacket(resp, P_FE2CL_REP_EP_RACE_START_SUCC);
}

static void racingGetPod(CNSocket* sock, CNPacketData* data) {
    if (EPRaces.find(sock) == EPRaces.end())
        return; 

    auto req = (sP_CL2FE_REQ_EP_GET_RING*)data->buf;

    if (EPRaces[sock].collectedRings.count(req->iRingLID))
        return; 

    // without an anticheat system, we really don't have a choice but to honor the request
    // TODO: proximity check so players can't cheat the race by replaying packets
    EPRaces[sock].collectedRings.insert(req->iRingLID);

    INITSTRUCT(sP_FE2CL_REP_EP_GET_RING_SUCC, resp);

    resp.iRingLID = req->iRingLID;
    resp.iRingCount_Get = EPRaces[sock].collectedRings.size();

    sock->sendPacket(resp, P_FE2CL_REP_EP_GET_RING_SUCC);
}

static void racingCancel(CNSocket* sock, CNPacketData* data) {
    if (EPRaces.find(sock) == EPRaces.end())
        return; // race not found

    Player* plr = PlayerManager::getPlayer(sock);
    EPRaces.erase(sock);

    INITSTRUCT(sP_FE2CL_REP_EP_RACE_CANCEL_SUCC, resp);
    sock->sendPacket(resp, P_FE2CL_REP_EP_RACE_CANCEL_SUCC);

    /*
     * This request packet is used for both cancelling the race via the
     * NPC at the start, *and* failing the race by running out of time.
     * If the latter is to happen, the client disables movement until it
     * receives a packet from the server that re-enables it.
     *
     * So, in order to prevent a potential softlock we respawn the player.
     */

    WarpLocation* respawnLoc = PlayerManager::getRespawnPoint(plr);

    if (respawnLoc != nullptr) {
        PlayerManager::sendPlayerTo(sock, respawnLoc->x, respawnLoc->y, respawnLoc->z, respawnLoc->instanceID);
    } else {
        // fallback, just respawn the player in-place if no suitable point is found
        PlayerManager::sendPlayerTo(sock, plr->x, plr->y, plr->z, plr->instanceID);
    }
}

static void racingEnd(CNSocket* sock, CNPacketData* data) {
    if (EPRaces.find(sock) == EPRaces.end())
        return; // race not found

    auto req = (sP_CL2FE_REQ_EP_RACE_END*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (NPCManager::NPCs.find(req->iEndEcomID) == NPCManager::NPCs.end())
        return; // finish line agent not found

    int mapNum = MAPNUM(NPCManager::NPCs[req->iEndEcomID]->instanceID);
    if (EPData.find(mapNum) == EPData.end() || EPData[mapNum].EPID == 0)
        return; // IZ not found

    EPInfo& epInfo = EPData[mapNum];
    EPRace& epRace = EPRaces[sock];

    uint64_t now = getTime() / 1000;
    int timeDiff = now - epRace.startTime;
    int podsCollected = epRace.collectedRings.size();

    int score = calculateRaceScore(epInfo, podsCollected, timeDiff);
    int fm = calculateFMReward(epInfo, podsCollected);

    // we submit the ranking first...
    handleRaceRanking(epInfo, plr, podsCollected, score, timeDiff, resp);

    // ...then we get the top ranking, which may or may not be what we just submitted
    Database::RaceRanking topRankingPlayer = Database::getTopRaceRanking(epInfo.EPID, plr->iID);

    INITSTRUCT(sP_FE2CL_REP_EP_RACE_END_SUCC, resp);

    // get rank scores and rewards
    std::vector<int>* rankScores = &EPRewards[epInfo.EPID].first;
    std::vector<int>* rankRewards = &EPRewards[epInfo.EPID].second;

    // top ranking
    int maxRank = rankScores->size() - 1;
    int topRank = 0;
    while (topRank < maxRank && rankScores->at(topRank) > topRankingPlayer.Score)
        topRank++;

    resp.iEPTopRank = topRank + 1;
    resp.iEPTopRingCount = topRankingPlayer.RingCount;
    resp.iEPTopScore = topRankingPlayer.Score;
    resp.iEPTopTime = topRankingPlayer.Time;

    // this ranking
    int rank = 0;
    while (rank < maxRank && rankScores->at(rank) > score)
        rank++;

    resp.iEPRank = rank + 1;
    resp.iEPRingCnt = podsCollected;
    resp.iEPScore = score;
    resp.iEPRaceTime = timeDiff;
    resp.iEPRaceMode = EPRaces[sock].mode;
    resp.iEPRewardFM = fm;

    Missions::updateFusionMatter(sock, resp.iEPRewardFM);

    resp.iFusionMatter = plr->fusionmatter;
    resp.iFatigue = 50;
    resp.iFatigue_Level = 1;

    EPRaces.erase(sock);
    sock->sendPacket(resp, P_FE2CL_REP_EP_RACE_END_SUCC);
}

void Racing::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_START, racingStart);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_GET_RING, racingGetPod);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_CANCEL, racingCancel);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_EP_RACE_END, racingEnd);
}
