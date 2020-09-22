#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "PlayerManager.hpp"
#include "TransportManager.hpp"

#include <unordered_map>

std::map<int32_t, TransportRoute> TransportManager::Routes;
std::map<int32_t, TransportLocation> TransportManager::Locations;
std::map<int32_t, std::queue<WarpLocation>> TransportManager::SkywayPaths;
std::unordered_map<CNSocket*, std::queue<WarpLocation>> TransportManager::SkywayQueues;

void TransportManager::init() {
    REGISTER_SHARD_TIMER(tickSkywaySystem, 1000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION, transportRegisterLocationHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION, transportWarpHandler);
}

void TransportManager::transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION))
        return; // malformed packet

    sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION* transport = (sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    bool newReg = false; // this is a new registration
    //std::cout << "request to register transport, eTT " << transport->eTT << ", locID " << transport->iLocationID << ", npc " << transport->iNPC_ID << std::endl;
    if (transport->eTT == 1) { // S.C.A.M.P.E.R.
        if (transport->iLocationID < 1 || transport->iLocationID > 31) { // sanity check
            std::cout << "[WARN] S.C.A.M.P.E.R. location ID " << transport->iLocationID << " is out of bounds" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);

            failResp.eTT = transport->eTT;
            failResp.iErrorCode = 0; // TODO: review what error code to use here
            failResp.iLocationID = transport->iLocationID;

            sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
            return;
        }

        // update registration bitfield using bitmask
        uint32_t newScamperFlag = plr->iWarpLocationFlag | (plr->IsGM ? UINT32_MAX : (1UL << (transport->iLocationID - 1)));
        if (newScamperFlag != plr->iWarpLocationFlag) {
            plr->iWarpLocationFlag = newScamperFlag;
            newReg = true;
        }
    } else if (transport->eTT == 2) { // Monkey Skyway System
        if (transport->iLocationID < 1 || transport->iLocationID > 127) { // sanity check
            std::cout << "[WARN] Skyway location ID " << transport->iLocationID << " is out of bounds" << std::endl;
            INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);

            failResp.eTT = transport->eTT;
            failResp.iErrorCode = 0; // TODO: review what error code to use here
            failResp.iLocationID = transport->iLocationID;

            sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
            return;
        }

        /*
         * assuming the two bitfields are just stuck together to make a longer one, do a similar operation
         */
        if (plr->IsGM) {
            plr->aSkywayLocationFlag[0] = UINT64_MAX;
            plr->aSkywayLocationFlag[1] = UINT64_MAX;
            newReg = true;
        } else {
            int index = transport->iLocationID > 64 ? 1 : 0;
            uint64_t newMonkeyFlag = plr->aSkywayLocationFlag[index] | (1ULL << (index ? transport->iLocationID - 65 : transport->iLocationID - 1));
            if (newMonkeyFlag != plr->aSkywayLocationFlag[index]) {
                plr->aSkywayLocationFlag[index] = newMonkeyFlag;
                newReg = true;
            }
        }
    } else {
        std::cout << "[WARN] Unknown mode of transport; eTT = " << transport->eTT << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);

        failResp.eTT = transport->eTT;
        failResp.iErrorCode = 0; // TODO: review what error code to use here
        failResp.iLocationID = transport->iLocationID;

        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL));
        return;
    }

    if (!newReg)
        return; // don't send new registration message

    INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, resp);
    // response parameters
    resp.eTT = transport->eTT;
    resp.iLocationID = transport->iLocationID;
    resp.iWarpLocationFlag = plr->iWarpLocationFlag;
    resp.aWyvernLocationFlag[0] = plr->aSkywayLocationFlag[0];
    resp.aWyvernLocationFlag[1] = plr->aSkywayLocationFlag[1];

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC));
}

void TransportManager::transportWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION* req = (sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    /*
     * req:
     * eIL -- inventory type
     * iNPC_ID -- the ID of the NPC who is warping you
     * iTransporationID -- iVehicleID
     * iSlotNum -- inventory slot number
     */

    if (Routes.find(req->iTransporationID) == Routes.end() || Routes[req->iTransporationID].cost > plr->money) { // sanity check
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL, failResp);

        failResp.iErrorCode = 0; // TODO: error code
        failResp.iTransportationID = req->iTransporationID;

        sock->sendPacket((void*)&failResp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL, sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL));
        return;
    }

    TransportRoute route = Routes[req->iTransporationID];
    plr->money -= route.cost;

    TransportLocation target;
    PlayerView& plrv = PlayerManager::players[sock];
    switch (route.type) {
    case 1: // S.C.A.M.P.E.R.
        target = Locations[route.end];
        plr->x = target.x;
        plr->y = target.y;
        plr->z = target.z;
        /*
         * Not strictly necessary since there isn't a valid SCAMPER that puts you in the
         * same map tile you were already in, but we might as well force an NPC reload.
         */
        PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
        plrv.currentChunks.clear();
        plrv.chunkPos = std::make_pair<int, int>(0, 0);
        break;
    case 2: // Monkey Skyway
        if (SkywayPaths.find(route.mssRouteNum) != SkywayPaths.end()) { // check if route exists
            SkywayQueues[sock] = SkywayPaths[route.mssRouteNum]; // set socket point queue to route
            break;
        }

        // refund and send alert packet
        plr->money += route.cost;
        INITSTRUCT(sP_FE2CL_ANNOUNCE_MSG, alert);
        alert.iAnnounceType = 0; // don't think this lets us make a confirm dialog
        alert.iDuringTime = 3;
        U8toU16("Skyway route " + std::to_string(route.mssRouteNum) + " isn't pathed yet. You will not be charged any taros.", (char16_t*)alert.szAnnounceMsg);
        sock->sendPacket((void*)&alert, P_FE2CL_ANNOUNCE_MSG, sizeof(sP_FE2CL_ANNOUNCE_MSG));

        std::cout << "[WARN] MSS route " << route.mssRouteNum << " not pathed" << std::endl;
        break;
    default:
        std::cout << "[WARN] Unknown tranportation type " << route.type << std::endl;
        break;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC, resp);
    // response parameters
    resp.eTT = route.type;
    resp.iCandy = plr->money;
    resp.iX = plr->x;
    resp.iY = plr->y;
    resp.iZ = plr->z;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC));
}

/*
 * Go through every socket that has broomstick points queued up, and advance to the next point.
 * If the player has disconnected or finished the route, clean up and remove them from the queue.
 */
void TransportManager::tickSkywaySystem(CNServer* serv, time_t currTime) {
    
    //std::cout << SkywayQueue.size();
    // using an unordered map so we can remove finished players in one iteration
    std::unordered_map<CNSocket*, std::queue<WarpLocation>>::iterator it = SkywayQueues.begin();
    while (it != SkywayQueues.end()) {

        std::queue<WarpLocation>* queue = &it->second;
        
        Player* plr = nullptr;
        if(PlayerManager::players.find(it->first) != PlayerManager::players.end()) // check if socket still has a player
            plr = PlayerManager::getPlayer(it->first);
        
        if (plr == nullptr) {
            // pluck out dead socket + update iterator
            it = SkywayQueues.erase(it);
            continue;
        }

        if (queue->empty()) {
            // send dismount packet
            INITSTRUCT(sP_FE2CL_REP_PC_RIDING_SUCC, rideSucc);
            INITSTRUCT(sP_FE2CL_PC_RIDING, rideBroadcast);
            rideSucc.iPC_ID = plr->iID;
            rideSucc.eRT = 0;
            rideBroadcast.iPC_ID = plr->iID;
            rideBroadcast.eRT = 0;
            it->first->sendPacket((void*)&rideSucc, P_FE2CL_REP_PC_RIDING_SUCC, sizeof(sP_FE2CL_REP_PC_RIDING_SUCC));
            // send packet to players in view
            PlayerManager::sendToViewable(it->first, (void*)&rideBroadcast, P_FE2CL_PC_RIDING, sizeof(sP_FE2CL_PC_RIDING));
            it = SkywayQueues.erase(it); // remove player from tracking map + update iterator
        } else {
            WarpLocation point = queue->front(); // get point
            queue->pop(); // remove point from front of queue

            INITSTRUCT(sP_FE2CL_PC_BROOMSTICK_MOVE, bmstk);
            bmstk.iPC_ID = plr->iID;
            bmstk.iToX = point.x;
            bmstk.iToY = point.y;
            bmstk.iToZ = point.z;
            it->first->sendPacket((void*)&bmstk, P_FE2CL_PC_BROOMSTICK_MOVE, sizeof(sP_FE2CL_PC_BROOMSTICK_MOVE));
            // set player location to point to update viewables
            PlayerManager::updatePlayerPosition(it->first, point.x, point.y, point.z);
            // send packet to players in view
            PlayerManager::sendToViewable(it->first, (void*)&bmstk, P_FE2CL_PC_BROOMSTICK_MOVE, sizeof(sP_FE2CL_PC_BROOMSTICK_MOVE));

            it++; // go to next entry in map
        }
    }
}
