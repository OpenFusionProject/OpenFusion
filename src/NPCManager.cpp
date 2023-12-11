#include "NPCManager.hpp"

#include "servers/CNShardServer.hpp"

#include "PlayerManager.hpp"
#include "Items.hpp"
#include "settings.hpp"
#include "Combat.hpp"
#include "Missions.hpp"
#include "Chunking.hpp"
#include "Nanos.hpp"
#include "TableData.hpp"
#include "Groups.hpp"
#include "Racing.hpp"
#include "Vendors.hpp"
#include "Abilities.hpp"
#include "Rand.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>
#include <assert.h>
#include <limits.h>

using namespace NPCManager;

std::unordered_map<int32_t, BaseNPC*> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;
nlohmann::json NPCManager::NPCData;

static std::queue<int32_t> RemovalQueue;

/*
 * Initialized at the end of TableData::init().
 * This allows us to summon and kill mobs in arbitrary order without
 * NPC ID collisions.
 */
int32_t NPCManager::nextId;

void NPCManager::destroyNPC(int32_t id) {
    // sanity check
    if (NPCs.find(id) == NPCs.end()) {
        std::cout << "npc not found: " << id << std::endl;
        return;
    }

    BaseNPC* entity = NPCs[id];

    // sanity check
    if (!Chunking::chunkExists(entity->chunkPos)) {
        std::cout << "chunk not found!" << std::endl;
        return;
    }

    // remove NPC from the chunk
    EntityRef ref = {id};
    Chunking::untrackEntity(entity->chunkPos, ref);

    // remove from viewable chunks
    Chunking::removeEntityFromChunks(Chunking::getViewableChunks(entity->chunkPos), ref);

    // finally, remove it from the map and free it
    NPCs.erase(id);
    delete entity;
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z, uint64_t I, int angle) {
    BaseNPC* npc = NPCs[id];
    npc->angle = angle;
    ChunkPos oldChunk = npc->chunkPos;
    ChunkPos newChunk = Chunking::chunkPosAt(X, Y, I);
    npc->x = X;
    npc->y = Y;
    npc->z = Z;
    npc->instanceID = I;
    if (oldChunk == newChunk)
        return; // didn't change chunks
    Chunking::updateEntityChunk({id}, oldChunk, newChunk);
}

void NPCManager::sendToViewable(Entity *npc, void *buf, uint32_t type, size_t size) {
    for (auto it = npc->viewableChunks.begin(); it != npc->viewableChunks.end(); it++) {
        Chunk* chunk = *it;
        for (const EntityRef& ref : chunk->entities) {
            if (ref.kind == EntityKind::PLAYER)
                ref.sock->sendPacket(buf, type, size);
        }
    }
}

static void npcBarkHandler(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_BARKER* req = (sP_CL2FE_REQ_BARKER*)data->buf;

    int taskID = req->iMissionTaskID;
    // ignore req->iNPC_ID as it is often fixated on a single npc in the region

    if (Missions::Tasks.find(taskID) == Missions::Tasks.end()) {
        std::cout << "mission task not found: " << taskID << std::endl;
        return;
    }

    TaskData* td = Missions::Tasks[taskID];
    auto& barks = td->task["m_iHBarkerTextID"];

    Player* plr = PlayerManager::getPlayer(sock);
    std::vector<std::pair<int32_t, int32_t>> npcLines;

    for (Chunk* chunk : plr->viewableChunks) {
        for (auto ent = chunk->entities.begin(); ent != chunk->entities.end(); ent++) {
            if (ent->kind == EntityKind::PLAYER)
                continue;

            BaseNPC* npc = (BaseNPC*)ent->getEntity();
            if (npc->type < 0 || npc->type >= NPCData.size())
                continue; // npc unknown ?!

            int barkType = NPCData[npc->type]["m_iBarkerType"];
            if (barkType < 1 || barkType > 4)
                continue; // no barks

            int barkID = barks[barkType - 1];
            if (barkID == 0)
                continue; // no barks

            npcLines.push_back(std::make_pair(npc->id, barkID));
        }
    }

    if (npcLines.size() == 0)
        return; // totally no barks

    auto& [npcID, missionStringID] = npcLines[Rand::rand(npcLines.size())];

    INITSTRUCT(sP_FE2CL_REP_BARKER, resp);
    resp.iNPC_ID = npcID;
    resp.iMissionStringID = missionStringID;
    sock->sendPacket(resp, P_FE2CL_REP_BARKER);
}

static void npcUnsummonHandler(CNSocket* sock, CNPacketData* data) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::destroyNPC(req->iNPC_ID);
}

// type must already be checked and updateNPCPosition() must be called on the result
BaseNPC *NPCManager::summonNPC(int spawnX, int spawnY, int spawnZ, uint64_t instance, int type, bool respawn, bool baseInstance) {
    uint64_t inst = baseInstance ? MAPNUM(instance) : instance;

    int id = nextId--;
    int team = NPCData[type]["m_iTeam"];
    BaseNPC *npc = nullptr;

    if (team == 2) {
        npc = new Mob(spawnX, spawnY, spawnZ, inst, type, NPCData[type], id);

        // re-enable respawning, if desired
        ((Mob*)npc)->summoned = !respawn;
    } else
        npc = new BaseNPC(0, inst, type, id);

    NPCs[id] = npc;

    return npc;
}

static void npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    auto req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int limit = NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (plr->accountLevel > 30 || req->iNPCType > limit || req->iNPCCnt > 100)
        return;

    for (int i = 0; i < req->iNPCCnt; i++) {
        BaseNPC *npc = summonNPC(plr->x, plr->y, plr->z, plr->instanceID, req->iNPCType);
        updateNPCPosition(npc->id, plr->x, plr->y, plr->z, plr->instanceID, 0);
    }
}

static void handleWarp(CNSocket* sock, int32_t warpId) {
    Player* plr = PlayerManager::getPlayer(sock);
    // sanity check
    if (Warps.find(warpId) == Warps.end())
        return;

    if (plr->iPCState & 8) {
        // remove the player's vehicle
        plr->iPCState &= ~8;

        // send to self
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, off);
        sock->sendPacket(off, P_FE2CL_PC_VEHICLE_OFF_SUCC);

        // send to others
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
        chg.iPC_ID = plr->iID;
        chg.iState = plr->iPCState;
        PlayerManager::sendToViewable(sock, chg, P_FE2CL_PC_STATE_CHANGE);
    }

    // std::cerr << "Warped to Map Num:" << Warps[warpId].instanceID << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance) {
        uint64_t instanceID = Warps[warpId].instanceID;

        Player* leader = plr;
        if (plr->group != nullptr) leader = PlayerManager::getPlayer(plr->group->filter(EntityKind::PLAYER)[0].sock);

        // if warp requires you to be on a mission, it's gotta be a unique instance
        if (Warps[warpId].limitTaskID != 0 || instanceID == 14) { // 14 is a special case for the Time Lab
            instanceID += ((uint64_t)leader->iID << 32); // upper 32 bits are leader ID
            Chunking::createInstance(instanceID);

            // save Lair entrance coords as a pseudo-Resurrect 'Em
            plr->recallX = Warps[warpId].x;
            plr->recallY = Warps[warpId].y;
            plr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
            plr->recallInstance = instanceID;
        }

        if (plr->group == nullptr)
            PlayerManager::sendPlayerTo(sock, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
        else {
            auto players = plr->group->filter(EntityKind::PLAYER);
            for (int i = 0; i < players.size(); i++) {
                CNSocket* sockTo = players[i].sock;
                Player* otherPlr = PlayerManager::getPlayer(sockTo);

                if (otherPlr == nullptr || sockTo == nullptr)
                    continue;

                // save Lair entrance coords for everyone else as well
                otherPlr->recallX = Warps[warpId].x;
                otherPlr->recallY = Warps[warpId].y;
                otherPlr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
                otherPlr->recallInstance = instanceID;

                // remove their vehicle if they're on one
                if (otherPlr->iPCState & 8) {
                    // remove the player's vehicle
                    otherPlr->iPCState &= ~8;

                    // send to self
                    INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, off);
                    sockTo->sendPacket(off, P_FE2CL_PC_VEHICLE_OFF_SUCC);

                    // send to others
                    INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
                    chg.iPC_ID = otherPlr->iID;
                    chg.iState = otherPlr->iPCState;
                    PlayerManager::sendToViewable(sockTo, chg, P_FE2CL_PC_STATE_CHANGE);
                }

                PlayerManager::sendPlayerTo(sockTo, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
            }
        }
    }
    else
    {
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp); // Can only be used for exiting instances because it sets the instance flag to false
        resp.iX = Warps[warpId].x;
        resp.iY = Warps[warpId].y;
        resp.iZ = Warps[warpId].z;
        resp.iCandy = plr->money;
        resp.eIL = 4; // do not take away any items
        uint64_t fromInstance = plr->instanceID; // pre-warp instance, saved for post-warp
        plr->instanceID = INSTANCE_OVERWORLD;
        Missions::failInstancedMissions(sock); // fail any instanced missions
        sock->sendPacket(resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC);

        PlayerManager::updatePlayerPositionForWarp(sock, resp.iX, resp.iY, resp.iZ, INSTANCE_OVERWORLD);

        // remove the player's ongoing race, if any
        if (Racing::EPRaces.find(sock) != Racing::EPRaces.end())
            Racing::EPRaces.erase(sock);

        // post-warp: check if the source instance has no more players in it and delete it if so
        Chunking::destroyInstanceIfEmpty(fromInstance);
    }
}

static void npcWarpHandler(CNSocket* sock, CNPacketData* data) {
    auto warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;
    handleWarp(sock, warpNpc->iWarpID);
}

static void npcWarpTimeMachine(CNSocket* sock, CNPacketData* data) {
    // this is just a warp request
    handleWarp(sock, 28);
}

/*
 * Helper function to get NPC closest to coordinates in specified chunks
 */
BaseNPC* NPCManager::getNearestNPC(std::set<Chunk*>* chunks, int X, int Y, int Z) {
    BaseNPC* npc = nullptr;
    int lastDist = INT_MAX;
    for (auto c = chunks->begin(); c != chunks->end(); c++) { // haha get it
        Chunk* chunk = *c;
        for (auto ent = chunk->entities.begin(); ent != chunk->entities.end(); ent++) {
            if (ent->kind == EntityKind::PLAYER)
                continue;

            BaseNPC* npcTemp = (BaseNPC*)ent->getEntity();
            int distXY = std::hypot(X - npcTemp->x, Y - npcTemp->y);
            int dist = std::hypot(distXY, Z - npcTemp->z);
            if (dist < lastDist) {
                npc = npcTemp;
                lastDist = dist;
            }
        }
    }
    return npc;
}

// TODO: Move this to separate file in ai/ subdir when implementing more events
#pragma region NPCEvents

// summon right arm and stage 2 body
static void lordFuseStageTwo(CombatNPC *npc) {
    Mob *oldbody = (Mob*)npc; // adaptium, stun

    std::cout << "Lord Fuse stage two" << std::endl;

    // Fuse doesn't move
    // Blastons, Heal
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->x, oldbody->y, oldbody->z, oldbody->instanceID, 2467);

    newbody->angle = oldbody->angle;
    NPCManager::updateNPCPosition(newbody->id, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        oldbody->instanceID, oldbody->angle);

    // right arm, Adaptium, Stun
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->x - 600, oldbody->y, oldbody->z, oldbody->instanceID, 2469);

    arm->angle = oldbody->angle;
    NPCManager::updateNPCPosition(arm->id, arm->spawnX, arm->spawnY, arm->spawnZ,
        oldbody->instanceID, oldbody->angle);
}

// summon left arm and stage 3 body
static void lordFuseStageThree(CombatNPC *npc) {
    Mob *oldbody = (Mob*)npc;

    std::cout << "Lord Fuse stage three" << std::endl;

    // Cosmix, Damage Point
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->x, oldbody->y, oldbody->z, oldbody->instanceID, 2468);

    newbody->angle = oldbody->angle;
    NPCManager::updateNPCPosition(newbody->id, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        newbody->instanceID, oldbody->angle);

    // Blastons, Heal
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->x + 600, oldbody->y, oldbody->z, oldbody->instanceID, 2470);

    arm->angle = oldbody->angle;
    NPCManager::updateNPCPosition(arm->id, arm->spawnX, arm->spawnY, arm->spawnZ,
        arm->instanceID, oldbody->angle);
}

std::vector<NPCEvent> NPCManager::NPCEvents = {
    NPCEvent(2466, AIState::DEAD, lordFuseStageTwo),
    NPCEvent(2467, AIState::DEAD, lordFuseStageThree),
};

#pragma endregion NPCEvents

void NPCManager::queueNPCRemoval(int32_t id) {
    RemovalQueue.push(id);
}

static void step(CNServer *serv, time_t currTime) {
    for (auto& pair : NPCs) {
        if (pair.second->kind != EntityKind::COMBAT_NPC && pair.second->kind != EntityKind::MOB)
            continue;
        auto npc = (CombatNPC*)pair.second;

        npc->step(currTime);
    }

    // deallocate all NPCs queued for removal
    while (RemovalQueue.size() > 0) {
        NPCManager::destroyNPC(RemovalQueue.front());
        RemovalQueue.pop();
    }
}

void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TIME_TO_GO_WARP, npcWarpTimeMachine);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_UNSUMMON, npcUnsummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);

    REGISTER_SHARD_TIMER(step, MS_PER_COMBAT_TICK);
}
