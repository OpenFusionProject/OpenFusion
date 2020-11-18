#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "PlayerManager.hpp"
#include "NanoManager.hpp"
#include "TransportManager.hpp"
#include "TableData.hpp"
#include "MobManager.hpp"

#include <unordered_map>
#include <cmath>

std::map<int32_t, TransportRoute> TransportManager::Routes;
std::map<int32_t, TransportLocation> TransportManager::Locations;
std::map<int32_t, std::queue<WarpLocation>> TransportManager::SkywayPaths;
std::unordered_map<CNSocket*, std::queue<WarpLocation>> TransportManager::SkywayQueues;
std::unordered_map<int32_t, std::queue<WarpLocation>> TransportManager::NPCQueues;

void TransportManager::init() {
    REGISTER_SHARD_TIMER(tickTransportationSystem, 1000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION, transportRegisterLocationHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION, transportWarpHandler);
}

void TransportManager::transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION))
        return; // malformed packet

    sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION* transport = (sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

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
        uint32_t newScamperFlag = plr->iWarpLocationFlag | (plr->accountLevel <= 40 ? INT32_MAX : (1UL << (transport->iLocationID - 1)));
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
        if (plr->accountLevel <= 40) {
            plr->aSkywayLocationFlag[0] = INT64_MAX;
            plr->aSkywayLocationFlag[1] = INT64_MAX;
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

    if (plr == nullptr)
        return;

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
    switch (route.type) {
    case 1: // S.C.A.M.P.E.R.
        target = Locations[route.end];
        PlayerManager::updatePlayerPosition(sock, target.x, target.y, target.z, INSTANCE_OVERWORLD, plr->angle);
        break;
    case 2: // Monkey Skyway
        if (SkywayPaths.find(route.mssRouteNum) != SkywayPaths.end()) { // check if route exists
            NanoManager::summonNano(sock, -1); // make sure that no nano is active during the ride
            SkywayQueues[sock] = SkywayPaths[route.mssRouteNum]; // set socket point queue to route
            break;
        } else if (TableData::RunningSkywayRoutes.find(route.mssRouteNum) != TableData::RunningSkywayRoutes.end()) {
            std::vector<WarpLocation>* _route = &TableData::RunningSkywayRoutes[route.mssRouteNum];

            NanoManager::summonNano(sock, -1);
            testMssRoute(sock, _route);
            break;
        }

        // refund and send alert packet
        plr->money += route.cost;
        INITSTRUCT(sP_FE2CL_ANNOUNCE_MSG, alert);
        alert.iAnnounceType = 0; // don't think this lets us make a confirm dialog
        alert.iDuringTime = 3;
        U8toU16("Skyway route " + std::to_string(route.mssRouteNum) + " isn't pathed yet. You will not be charged any taros.", (char16_t*)alert.szAnnounceMsg, sizeof(alert.szAnnounceMsg));
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

void TransportManager::testMssRoute(CNSocket *sock, std::vector<WarpLocation>* route) {
    int speed = 1500; // TODO: make this adjustable
    std::queue<WarpLocation> path;
    WarpLocation last = route->front(); // start pos

    for (int i = 1; i < route->size(); i++) {
        WarpLocation coords = route->at(i);
        TransportManager::lerp(&path, last, coords, speed);
        path.push(coords); // add keyframe to the queue
        last = coords; // update start pos
    }

    SkywayQueues[sock] = path;
}

void TransportManager::tickTransportationSystem(CNServer* serv, time_t currTime) {
    stepNPCPathing();
    stepSkywaySystem();
}

/*
 * Go through every socket that has broomstick points queued up, and advance to the next point.
 * If the player has disconnected or finished the route, clean up and remove them from the queue.
 */
void TransportManager::stepSkywaySystem() {

    // using an unordered map so we can remove finished players in one iteration
    std::unordered_map<CNSocket*, std::queue<WarpLocation>>::iterator it = SkywayQueues.begin();
    while (it != SkywayQueues.end()) {

        std::queue<WarpLocation>* queue = &it->second;

        Player* plr = PlayerManager::getPlayer(it->first);

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
            PlayerManager::updatePlayerPosition(it->first, point.x, point.y, point.z, plr->instanceID, plr->angle);
            // send packet to players in view
            PlayerManager::sendToViewable(it->first, (void*)&bmstk, P_FE2CL_PC_BROOMSTICK_MOVE, sizeof(sP_FE2CL_PC_BROOMSTICK_MOVE));

            it++; // go to next entry in map
        }
    }
}

void TransportManager::stepNPCPathing() {

    // all NPC pathing queues
    std::unordered_map<int32_t, std::queue<WarpLocation>>::iterator it = NPCQueues.begin();
    while (it != NPCQueues.end()) {

        std::queue<WarpLocation>* queue = &it->second;

        BaseNPC* npc = nullptr;
        if (NPCManager::NPCs.find(it->first) != NPCManager::NPCs.end())
            npc = NPCManager::NPCs[it->first];

        if (npc == nullptr || queue->empty()) {
            // pluck out dead path + update iterator
            it = NPCQueues.erase(it);
            continue;
        }

        // skip if not simulating mobs
        if (npc->npcClass == NPC_MOB && !MobManager::simulateMobs) {
            it++;
            continue;
        }

        // do not roam if not roaming
        if (npc->npcClass == NPC_MOB && ((Mob*)npc)->state != MobState::ROAMING) {
            it++;
            continue;
        }

        WarpLocation point = queue->front(); // get point
        queue->pop(); // remove point from front of queue

        // calculate displacement
        int dXY = hypot(point.x - npc->appearanceData.iX, point.y - npc->appearanceData.iY); // XY plane distance
        int distanceBetween = hypot(dXY, point.z - npc->appearanceData.iZ); // total distance

        // update NPC location to update viewables
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, point.x, point.y, point.z, npc->instanceID, npc->appearanceData.iAngle);

        switch (npc->npcClass) {
        case NPC_BUS:
            INITSTRUCT(sP_FE2CL_TRANSPORTATION_MOVE, busMove);
            busMove.eTT = 3;
            busMove.iT_ID = npc->appearanceData.iNPC_ID;
            busMove.iMoveStyle = 0; // ???
            busMove.iToX = point.x;
            busMove.iToY = point.y;
            busMove.iToZ = point.z;
            busMove.iSpeed = distanceBetween; // set to distance to match how monkeys work

            NPCManager::sendToViewable(npc, &busMove, P_FE2CL_TRANSPORTATION_MOVE, sizeof(sP_FE2CL_TRANSPORTATION_MOVE));
            break;
        case NPC_MOB:
            MobManager::incNextMovement((Mob*)npc);
            /* fallthrough */
        default:
            INITSTRUCT(sP_FE2CL_NPC_MOVE, move);
            move.iNPC_ID = npc->appearanceData.iNPC_ID;
            move.iMoveStyle = 0; // ???
            move.iToX = point.x;
            move.iToY = point.y;
            move.iToZ = point.z;
            move.iSpeed = distanceBetween;

            NPCManager::sendToViewable(npc, &move, P_FE2CL_NPC_MOVE, sizeof(sP_FE2CL_NPC_MOVE));
            break;
        }

        /*
         * Move processed point to the back to maintain cycle, unless this is a
         * dynamically calculated mob route.
         */
        if (!(npc->npcClass == NPC_MOB && !((Mob*)npc)->staticPath))
            queue->push(point);

        it++; // go to next entry in map
    }
}

/*
 * Linearly interpolate between two points and insert the results into a queue.
 */
void TransportManager::lerp(std::queue<WarpLocation>* queue, WarpLocation start, WarpLocation end, int gapSize, float curve) {
    int dXY = hypot(end.x - start.x, end.y - start.y); // XY plane distance
    int distanceBetween = hypot(dXY, end.z - start.z); // total distance
    int lerps = distanceBetween / gapSize; // number of intermediate points to add
    for (int i = 1; i <= lerps; i++) {
        WarpLocation lerp;
        // lerp math
        //float frac = i / (lerps + 1);
        float frac = powf(i, curve) / powf(lerps + 1, curve);
        lerp.x = (start.x * (1.0f - frac)) + (end.x * frac);
        lerp.y = (start.y * (1.0f - frac)) + (end.y * frac);
        lerp.z = (start.z * (1.0f - frac)) + (end.z * frac);
        queue->push(lerp); // add lerp'd point
    }
}
void TransportManager::lerp(std::queue<WarpLocation>* queue, WarpLocation start, WarpLocation end, int gapSize) {
    lerp(queue, start, end, gapSize, 1);
}
