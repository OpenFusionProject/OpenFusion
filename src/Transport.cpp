#include "servers/CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "Nanos.hpp"
#include "Transport.hpp"
#include "TableData.hpp"
#include "Combat.hpp"
#include "MobAI.hpp"

#include <unordered_map>
#include <cmath>

using namespace Transport;

std::map<int32_t, TransportRoute> Transport::Routes;
std::map<int32_t, TransportLocation> Transport::Locations;
std::vector<NPCPath> Transport::NPCPaths;
std::map<int32_t, std::queue<Vec3>> Transport::SkywayPaths;
std::unordered_map<CNSocket*, std::queue<Vec3>> Transport::SkywayQueues;
std::unordered_map<int32_t, std::queue<Vec3>> Transport::NPCQueues;

static void transportRegisterLocationHandler(CNSocket* sock, CNPacketData* data) {
    auto transport = (sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION*)data->buf;
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

            sock->sendPacket(failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL);
            return;
        }

        // update registration bitfield using bitmask
        uint32_t newScamperFlag = plr->iWarpLocationFlag | (1UL << (transport->iLocationID - 1));
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

            sock->sendPacket(failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL);
            return;
        }

        /*
         * assuming the two bitfields are just stuck together to make a longer one, do a similar operation
         */
        int index = transport->iLocationID > 64 ? 1 : 0;
        uint64_t newMonkeyFlag = plr->aSkywayLocationFlag[index] | (1ULL << (index ? transport->iLocationID - 65 : transport->iLocationID - 1));
        if (newMonkeyFlag != plr->aSkywayLocationFlag[index]) {
            plr->aSkywayLocationFlag[index] = newMonkeyFlag;
            newReg = true;
        }
    } else {
        std::cout << "[WARN] Unknown mode of transport; eTT = " << transport->eTT << std::endl;
        INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL, failResp);

        failResp.eTT = transport->eTT;
        failResp.iErrorCode = 0; // TODO: review what error code to use here
        failResp.iLocationID = transport->iLocationID;

        sock->sendPacket(failResp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL);
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

    sock->sendPacket(resp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC);
}

static void transportWarpHandler(CNSocket* sock, CNPacketData* data) {
    auto req = (sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION*)data->buf;
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

        sock->sendPacket(failResp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL);
        return;
    }

    TransportRoute route = Routes[req->iTransporationID];
    plr->money -= route.cost;

    TransportLocation* target = nullptr;
    switch (route.type) {
    case 1: // S.C.A.M.P.E.R.
        target = &Locations[route.end];
        break;
    case 2: // Monkey Skyway
        // set last safe coords
        plr->lastX = plr->x;
        plr->lastY = plr->y;
        plr->lastZ = plr->z;
        if (SkywayPaths.find(route.mssRouteNum) != SkywayPaths.end()) { // check if route exists
            Nanos::summonNano(sock, -1); // make sure that no nano is active during the ride
            SkywayQueues[sock] = SkywayPaths[route.mssRouteNum]; // set socket point queue to route
            plr->onMonkey = true;
            break;
        } else if (TableData::RunningSkywayRoutes.find(route.mssRouteNum) != TableData::RunningSkywayRoutes.end()) {
            std::vector<Vec3>* _route = &TableData::RunningSkywayRoutes[route.mssRouteNum];
            Nanos::summonNano(sock, -1);
            testMssRoute(sock, _route);
            plr->onMonkey = true;
            break;
        }

        // refund and send alert packet
        plr->money += route.cost;
        INITSTRUCT(sP_FE2CL_ANNOUNCE_MSG, alert);
        alert.iAnnounceType = 0; // don't think this lets us make a confirm dialog
        alert.iDuringTime = 3;
        U8toU16("Skyway route " + std::to_string(route.mssRouteNum) + " isn't pathed yet. You will not be charged any taros.", (char16_t*)alert.szAnnounceMsg, sizeof(alert.szAnnounceMsg));
        sock->sendPacket(alert, P_FE2CL_ANNOUNCE_MSG);

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
    resp.iX = (target == nullptr) ? plr->x : target->x;
    resp.iY = (target == nullptr) ? plr->y : target->y;
    resp.iZ = (target == nullptr) ? plr->z : target->z;
    sock->sendPacket(resp, P_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC);

    if (target == nullptr)
        return;
    // we warped; update position and chunks
    Chunking::updateEntityChunk({sock}, plr->chunkPos, std::make_tuple(0, 0, 0)); // force player to reload chunks
    PlayerManager::updatePlayerPosition(sock, target->x, target->y, target->z, INSTANCE_OVERWORLD, plr->angle);
}

void Transport::testMssRoute(CNSocket *sock, std::vector<Vec3>* route) {
    int speed = 1500; // TODO: make this adjustable
    std::queue<Vec3> path;
    Vec3 last = route->front(); // start pos

    for (int i = 1; i < route->size(); i++) {
        Vec3 coords = route->at(i);
        Transport::lerp(&path, last, coords, speed);
        path.push(coords); // add keyframe to the queue
        last = coords; // update start pos
    }

    SkywayQueues[sock] = path;
}

/*
 * Go through every socket that has broomstick points queued up, and advance to the next point.
 * If the player has disconnected or finished the route, clean up and remove them from the queue.
 */
static void stepSkywaySystem() {

    // using an unordered map so we can remove finished players in one iteration
    std::unordered_map<CNSocket*, std::queue<Vec3>>::iterator it = SkywayQueues.begin();
    while (it != SkywayQueues.end()) {

        std::queue<Vec3>* queue = &it->second;

        if (PlayerManager::players.find(it->first) == PlayerManager::players.end()) {
            // pluck out dead socket + update iterator
            it = SkywayQueues.erase(it);
            continue;
        }

        Player* plr = PlayerManager::getPlayer(it->first);

        if (queue->empty()) {
            // send dismount packet
            INITSTRUCT(sP_FE2CL_REP_PC_RIDING_SUCC, rideSucc);
            INITSTRUCT(sP_FE2CL_PC_RIDING, rideBroadcast);
            rideSucc.iPC_ID = plr->iID;
            rideSucc.eRT = 0;
            rideBroadcast.iPC_ID = plr->iID;
            rideBroadcast.eRT = 0;
            it->first->sendPacket(rideSucc, P_FE2CL_REP_PC_RIDING_SUCC);
            // send packet to players in view
            PlayerManager::sendToViewable(it->first, rideBroadcast, P_FE2CL_PC_RIDING);
            it = SkywayQueues.erase(it); // remove player from tracking map + update iterator
            plr->onMonkey = false;
        } else {
            Vec3 point = queue->front(); // get point
            queue->pop(); // remove point from front of queue

            INITSTRUCT(sP_FE2CL_PC_BROOMSTICK_MOVE, bmstk);
            bmstk.iPC_ID = plr->iID;
            bmstk.iToX = point.x;
            bmstk.iToY = point.y;
            bmstk.iToZ = point.z;
            it->first->sendPacket(bmstk, P_FE2CL_PC_BROOMSTICK_MOVE);
            // set player location to point to update viewables
            PlayerManager::updatePlayerPosition(it->first, point.x, point.y, point.z, plr->instanceID, plr->angle);
            // send packet to players in view
            PlayerManager::sendToViewable(it->first, bmstk, P_FE2CL_PC_BROOMSTICK_MOVE);

            it++; // go to next entry in map
        }
    }
}

static void stepNPCPathing() {

    // all NPC pathing queues
    std::unordered_map<int32_t, std::queue<Vec3>>::iterator it = NPCQueues.begin();
    while (it != NPCQueues.end()) {

        std::queue<Vec3>* queue = &it->second;

        BaseNPC* npc = nullptr;
        if (NPCManager::NPCs.find(it->first) != NPCManager::NPCs.end())
            npc = NPCManager::NPCs[it->first];

        if (npc == nullptr || queue->empty()) {
            // pluck out dead path + update iterator
            it = NPCQueues.erase(it);
            continue;
        }

        // skip if not simulating mobs
        if (npc->type == EntityType::MOB && !MobAI::simulateMobs) {
            it++;
            continue;
        }

        // do not roam if not roaming
        if (npc->type == EntityType::MOB && ((Mob*)npc)->state != MobState::ROAMING) {
            it++;
            continue;
        }

        Vec3 point = queue->front(); // get point
        queue->pop(); // remove point from front of queue

        // calculate displacement
        int dXY = hypot(point.x - npc->x, point.y - npc->y); // XY plane distance
        int distanceBetween = hypot(dXY, point.z - npc->z); // total distance

        // update NPC location to update viewables
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, point.x, point.y, point.z, npc->instanceID, npc->appearanceData.iAngle);

        // TODO: move walking logic into Entity stack
        switch (npc->type) {
        case EntityType::BUS:
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
        case EntityType::MOB:
            MobAI::incNextMovement((Mob*)npc);
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
         * If this path should be repeated, move processed point to the back to maintain cycle.
         */
        if (npc->loopingPath)
            queue->push(point);

        it++; // go to next entry in map
    }
}

static void tickTransportationSystem(CNServer* serv, time_t currTime) {
    stepNPCPathing();
    stepSkywaySystem();
}

/*
 * Linearly interpolate between two points and insert the results into a queue.
 */
void Transport::lerp(std::queue<Vec3>* queue, Vec3 start, Vec3 end, int gapSize, float curve) {
    int dXY = hypot(end.x - start.x, end.y - start.y); // XY plane distance
    int distanceBetween = hypot(dXY, end.z - start.z); // total distance
    int lerps = distanceBetween / gapSize; // number of intermediate points to add
    for (int i = 1; i <= lerps; i++) {
        Vec3 lerp;
        // lerp math
        //float frac = i / (lerps + 1);
        float frac = powf(i, curve) / powf(lerps + 1, curve);
        lerp.x = (start.x * (1.0f - frac)) + (end.x * frac);
        lerp.y = (start.y * (1.0f - frac)) + (end.y * frac);
        lerp.z = (start.z * (1.0f - frac)) + (end.z * frac);
        queue->push(lerp); // add lerp'd point
    }
}
void Transport::lerp(std::queue<Vec3>* queue, Vec3 start, Vec3 end, int gapSize) {
    lerp(queue, start, end, gapSize, 1);
}

/*
 * Find and return the first path that targets either the type or the ID.
 * If no matches are found, return nullptr
 */
NPCPath* Transport::findApplicablePath(int32_t id, int32_t type, int taskID) {
    NPCPath* match = nullptr;
    for (auto _path = Transport::NPCPaths.begin(); _path != Transport::NPCPaths.end(); _path++) {

        // task ID for the path must match so escorts don't start early
        if (_path->escortTaskID != taskID)
            continue;

        // search target IDs
        for (int32_t pID : _path->targetIDs) {
            if (id == pID) {
                match = &(*_path);
                break;
            }
        }

        if (match != nullptr)
            break; // early break for ID matches, since ID has higher priority than type

        // search target types
        for (int32_t pType : _path->targetTypes) {
            if (type == pType) {
                match = &(*_path);
                break;
            }
        }

        if (match != nullptr)
            break;
    }
    return match;
}

void Transport::constructPathNPC(int32_t id, NPCPath* path) {
    BaseNPC* npc = NPCManager::NPCs[id];
    if (npc->type == EntityType::MOB)
        ((Mob*)(npc))->staticPath = true;
    npc->loopingPath = path->isLoop;

    // Interpolate
    std::vector<Vec3> pathPoints = path->points;
    std::queue<Vec3> points;

    auto _point = pathPoints.begin();
    Vec3 from = *_point; // point A coords
    for (_point++; _point != pathPoints.end(); _point++) { // loop through all point Bs
        Vec3 to = *_point; // point B coords
        // add point A to the queue
        if (path->isRelative) {
            // relative; the NPCs current position is assumed to be its spawn point
            Vec3 fromReal = { from.x + npc->x, from.y + npc->y, from.z + npc->z };
            Vec3 toReal = { to.x + npc->x, to.y + npc->y, to.z + npc->z };
            points.push(fromReal);
            Transport::lerp(&points, fromReal, toReal, path->speed); // lerp from A to B
        }
        else {
            // absolute
            points.push(from);
            Transport::lerp(&points, from, to, path->speed); // lerp from A to B
        }

        from = to; // update point A
    }

    Transport::NPCQueues[id] = points;
}

void Transport::init() {
    REGISTER_SHARD_TIMER(tickTransportationSystem, 1000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION, transportRegisterLocationHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION, transportWarpHandler);
}
