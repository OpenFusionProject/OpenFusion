#include "NPCManager.hpp"
#include "Items.hpp"
#include "settings.hpp"
#include "Combat.hpp"
#include "Missions.hpp"
#include "Chunking.hpp"
#include "Nanos.hpp"
#include "TableData.hpp"
#include "Groups.hpp"
#include "Racing.hpp"
#include "Vendor.hpp"
#include "Abilities.hpp"

#include <cmath>
#include <algorithm>
#include <list>
#include <fstream>
#include <vector>
#include <assert.h>
#include <limits.h>

#include "JSON.hpp"

using namespace NPCManager;

std::map<int32_t, BaseNPC*> NPCManager::NPCs;
std::map<int32_t, WarpLocation> NPCManager::Warps;
std::vector<WarpLocation> NPCManager::RespawnPoints;
/// sock, CBFlag -> until
std::map<std::pair<CNSocket*, int32_t>, time_t> NPCManager::EggBuffs;
std::unordered_map<int, EggType> NPCManager::EggTypes;
std::unordered_map<int, Egg*> NPCManager::Eggs;
nlohmann::json NPCManager::NPCData;

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
    Chunking::untrackNPC(entity->chunkPos, id);

    // remove from viewable chunks
    Chunking::removeNPCFromChunks(Chunking::getViewableChunks(entity->chunkPos), id);

    // remove from mob manager
    if (MobAI::Mobs.find(id) != MobAI::Mobs.end())
        MobAI::Mobs.erase(id);

    // remove from eggs
    if (Eggs.find(id) != Eggs.end())
        Eggs.erase(id);

    // finally, remove it from the map and free it
    delete entity->viewableChunks;
    NPCs.erase(id);
    delete entity;
}

void NPCManager::updateNPCPosition(int32_t id, int X, int Y, int Z, uint64_t I, int angle) {
    BaseNPC* npc = NPCs[id];
    npc->appearanceData.iAngle = angle;
    ChunkPos oldChunk = npc->chunkPos;
    ChunkPos newChunk = Chunking::chunkPosAt(X, Y, I);
    npc->appearanceData.iX = X;
    npc->appearanceData.iY = Y;
    npc->appearanceData.iZ = Z;
    npc->instanceID = I;
    if (oldChunk == newChunk)
        return; // didn't change chunks
    Chunking::updateNPCChunk(id, oldChunk, newChunk);
}

void NPCManager::sendToViewable(BaseNPC *npc, void *buf, uint32_t type, size_t size) {
    for (auto it = npc->viewableChunks->begin(); it != npc->viewableChunks->end(); it++) {
        Chunk* chunk = *it;
        for (CNSocket *s : chunk->players) {
            s->sendPacket(buf, type, size);
        }
    }
}

static void npcBarkHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_BARKER))
        return; // malformed packet

    sP_CL2FE_REQ_BARKER* req = (sP_CL2FE_REQ_BARKER*)data->buf;

    // get bark IDs from task data
    TaskData* td = Missions::Tasks[req->iMissionTaskID];
    std::vector<int> barks;
    for (int i = 0; i < 4; i++) {
        if (td->task["m_iHBarkerTextID"][i] != 0) // non-zeroes only
            barks.push_back(td->task["m_iHBarkerTextID"][i]);
    }

    if (barks.empty())
        return; // no barks

    INITSTRUCT(sP_FE2CL_REP_BARKER, resp);
    resp.iNPC_ID = req->iNPC_ID;
    resp.iMissionStringID = barks[rand() % barks.size()];
    sock->sendPacket((void*)&resp, P_FE2CL_REP_BARKER, sizeof(sP_FE2CL_REP_BARKER));
}

static void npcUnsummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_UNSUMMON))
        return; // malformed packet

    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_REQ_NPC_UNSUMMON* req = (sP_CL2FE_REQ_NPC_UNSUMMON*)data->buf;
    NPCManager::destroyNPC(req->iNPC_ID);
}

// type must already be checked and updateNPCPosition() must be called on the result
BaseNPC *NPCManager::summonNPC(int x, int y, int z, uint64_t instance, int type, bool respawn, bool baseInstance) {
    uint64_t inst = baseInstance ? MAPNUM(instance) : instance;
#define EXTRA_HEIGHT 0

    assert(nextId < INT32_MAX);
    int id = nextId++;
    int team = NPCData[type]["m_iTeam"];
    BaseNPC *npc = nullptr;

    if (team == 2) {
        npc = new Mob(x, y, z + EXTRA_HEIGHT, inst, type, NPCData[type], id);
        MobAI::Mobs[id] = (Mob*)npc;

        // re-enable respawning, if desired
        ((Mob*)npc)->summoned = !respawn;
    } else
        npc = new BaseNPC(x, y, z + EXTRA_HEIGHT, 0, inst, type, id);

    NPCs[id] = npc;

    return npc;
}

static void npcSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NPC_SUMMON))
        return; // malformed packet

    sP_CL2FE_REQ_NPC_SUMMON* req = (sP_CL2FE_REQ_NPC_SUMMON*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int limit = NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (plr->accountLevel > 30 || req->iNPCType > limit || req->iNPCCnt > 100)
        return;

    for (int i = 0; i < req->iNPCCnt; i++) {
        BaseNPC *npc = summonNPC(plr->x, plr->y, plr->z, plr->instanceID, req->iNPCType);
        updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, plr->instanceID, 0);
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
        sock->sendPacket((void*)&off, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));

        // send to others
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
        chg.iPC_ID = plr->iID;
        chg.iState = plr->iPCState;
        PlayerManager::sendToViewable(sock, (void*)&chg, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
    }

    // std::cerr << "Warped to Map Num:" << Warps[warpId].instanceID << " NPC ID " << Warps[warpId].npcID << std::endl;
    if (Warps[warpId].isInstance) {
        uint64_t instanceID = Warps[warpId].instanceID;

        // if warp requires you to be on a mission, it's gotta be a unique instance
        if (Warps[warpId].limitTaskID != 0 || instanceID == 14) { // 14 is a special case for the Time Lab
            instanceID += ((uint64_t)plr->iIDGroup << 32); // upper 32 bits are leader ID
            Chunking::createInstance(instanceID);

            // save Lair entrance coords as a pseudo-Resurrect 'Em
            plr->recallX = Warps[warpId].x;
            plr->recallY = Warps[warpId].y;
            plr->recallZ = Warps[warpId].z + RESURRECT_HEIGHT;
            plr->recallInstance = instanceID;
        }

        if (plr->iID == plr->iIDGroup && plr->groupCnt == 1)
            PlayerManager::sendPlayerTo(sock, Warps[warpId].x, Warps[warpId].y, Warps[warpId].z, instanceID);
        else {
            Player* leaderPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            for (int i = 0; i < leaderPlr->groupCnt; i++) {
                Player* otherPlr = PlayerManager::getPlayerFromID(leaderPlr->groupIDs[i]);
                CNSocket* sockTo = PlayerManager::getSockFromID(leaderPlr->groupIDs[i]);

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
                    sockTo->sendPacket((void*)&off, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));

                    // send to others
                    INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, chg);
                    chg.iPC_ID = otherPlr->iID;
                    chg.iState = otherPlr->iPCState;
                    PlayerManager::sendToViewable(sockTo, (void*)&chg, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
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
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));

        Chunking::updatePlayerChunk(sock, plr->chunkPos, std::make_tuple(0, 0, 0)); // force player to reload chunks
        PlayerManager::updatePlayerPosition(sock, resp.iX, resp.iY, resp.iZ, INSTANCE_OVERWORLD, plr->angle);

        // remove the player's ongoing race, if any
        if (Racing::EPRaces.find(sock) != Racing::EPRaces.end())
            Racing::EPRaces.erase(sock);

        // post-warp: check if the source instance has no more players in it and delete it if so
        Chunking::destroyInstanceIfEmpty(fromInstance);
    }
}

static void npcWarpHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC))
        return; // malformed packet

    sP_CL2FE_REQ_PC_WARP_USE_NPC* warpNpc = (sP_CL2FE_REQ_PC_WARP_USE_NPC*)data->buf;
    handleWarp(sock, warpNpc->iWarpID);
}

static void npcWarpTimeMachine(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_TIME_TO_GO_WARP))
        return; // malformed packet
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
        for (auto _npc = chunk->NPCs.begin(); _npc != chunk->NPCs.end(); _npc++) {
            BaseNPC* npcTemp = NPCs[*_npc];
            int distXY = std::hypot(X - npcTemp->appearanceData.iX, Y - npcTemp->appearanceData.iY);
            int dist = std::hypot(distXY, Z - npcTemp->appearanceData.iZ);
            if (dist < lastDist) {
                npc = npcTemp;
                lastDist = dist;
            }
        }
    }
    return npc;
}

int NPCManager::eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration) {
    Player* plr = PlayerManager::getPlayer(sock);
    Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

    int bitFlag = Groups::getGroupFlags(otherPlr);
    int CBFlag = Nanos::applyBuff(sock, skillId, 1, 3, bitFlag);

    size_t resplen; 

    if (skillId == 183) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Damage);
    } else if (skillId == 150) {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Heal_HP);
    } else {
        resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + sizeof(sSkillResult_Buff);
    }
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    // we know it's only one trailing struct, so we can skip full validation

    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    sP_FE2CL_NPC_SKILL_HIT* skillUse = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;

    if (skillId == 183) { // damage egg
        sSkillResult_Damage* skill = (sSkillResult_Damage*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iDamage = PC_MAXHEALTH(plr->level) * Nanos::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP -= skill->iDamage;
        if (plr->HP < 0)
            plr->HP = 0;
        skill->iHP = plr->HP;
    } else if (skillId == 150) { // heal egg
        sSkillResult_Heal_HP* skill = (sSkillResult_Heal_HP*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iHealHP = PC_MAXHEALTH(plr->level) * Nanos::SkillTable[skillId].powerIntensity[0] / 1000;
        plr->HP += skill->iHealHP;
        if (plr->HP > PC_MAXHEALTH(plr->level))
            plr->HP = PC_MAXHEALTH(plr->level);
        skill->iHP = plr->HP;
    } else { // regular buff egg
        sSkillResult_Buff* skill = (sSkillResult_Buff*)(respbuf + sizeof(sP_FE2CL_NPC_SKILL_HIT));
        memset(respbuf, 0, resplen);
        skill->eCT = 1;
        skill->iID = plr->iID;
        skill->iConditionBitFlag = plr->iConditionBitFlag;
    }

    skillUse->iNPC_ID = eggId;
    skillUse->iSkillID = skillId;
    skillUse->eST = Nanos::SkillTable[skillId].skillType;
    skillUse->iTargetCnt = 1;

    sock->sendPacket((void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_NPC_SKILL_HIT, resplen);

    if (CBFlag == 0)
        return -1;

    std::pair<CNSocket*, int32_t> key = std::make_pair(sock, CBFlag);

    // save the buff serverside;
    // if you get the same buff again, new duration will override the previous one
    time_t until = getTime() + (time_t)duration * 1000;
    EggBuffs[key] = until;

    return 0;
}

static void eggStep(CNServer* serv, time_t currTime) {
    // tick buffs
    time_t timeStamp = currTime;
    auto it = EggBuffs.begin();
    while (it != EggBuffs.end()) {
        // check remaining time
        if (it->second > timeStamp)
            it++;
        else { // if time reached 0
            CNSocket* sock = it->first.first;
            int32_t CBFlag = it->first.second;
            Player* plr = PlayerManager::getPlayer(sock);
            Player* otherPlr = PlayerManager::getPlayerFromID(plr->iIDGroup);

            int groupFlags = Groups::getGroupFlags(otherPlr);
            for (auto& pwr : Nanos::NanoPowers) {
                if (pwr.bitFlag == CBFlag) { // pick the power with the right flag and unbuff
                    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, resp);
                    resp.eCSTB = pwr.timeBuffID;
                    resp.eTBU = 2;
                    resp.eTBT = 3; // for egg buffs
                    plr->iConditionBitFlag &= ~CBFlag;
                    resp.iConditionBitFlag = plr->iConditionBitFlag |= groupFlags | plr->iSelfConditionBitFlag;
                    sock->sendPacket((void*)&resp, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));

                    INITSTRUCT(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT, resp2); // send a buff timeout to other players
                    resp2.eCT = 1;
                    resp2.iID = plr->iID;
                    resp2.iConditionBitFlag = plr->iConditionBitFlag;
                    PlayerManager::sendToViewable(sock, (void*)&resp2, P_FE2CL_CHAR_TIME_BUFF_TIME_OUT, sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT));
                }
            }
            // remove buff from the map
            it = EggBuffs.erase(it);
        }
    }

    // check dead eggs and eggs in inactive chunks
    for (auto egg : Eggs) {
        if (!egg.second->dead || !Chunking::inPopulatedChunks(egg.second->viewableChunks))
            continue;
        if (egg.second->deadUntil <= timeStamp) {
            // respawn it
            egg.second->dead = false;
            egg.second->deadUntil = 0;
            egg.second->appearanceData.iHP = 400;
            
            Chunking::addNPCToChunks(Chunking::getViewableChunks(egg.second->chunkPos), egg.first);
        }
    }

}

void NPCManager::npcDataToEggData(sNPCAppearanceData* npc, sShinyAppearanceData* egg) {
    egg->iX = npc->iX;
    egg->iY = npc->iY;
    egg->iZ = npc->iZ;
    // client doesn't care about egg->iMapNum
    egg->iShinyType = npc->iNPCType;
    egg->iShiny_ID = npc->iNPC_ID;
}

static void eggPickup(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SHINY_PICKUP))
        return; // malformed packet

    sP_CL2FE_REQ_SHINY_PICKUP* pickup = (sP_CL2FE_REQ_SHINY_PICKUP*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    int eggId = pickup->iShinyID;

    if (Eggs.find(eggId) == Eggs.end()) {
        std::cout << "[WARN] Player tried to open non existing egg?!" << std::endl;
        return;
    }
    Egg* egg = Eggs[eggId];

    if (egg->dead) {
        std::cout << "[WARN] Player tried to open a dead egg?!" << std::endl;
        return;
    }

    /* this has some issues with position desync, leaving it out for now
    if (abs(egg->appearanceData.iX - plr->x)>500 || abs(egg->appearanceData.iY - plr->y) > 500) {
        std::cout << "[WARN] Player tried to open an egg from the other chunk?!" << std::endl;
        return;
    }
    */

    int typeId = egg->appearanceData.iNPCType;
    if (EggTypes.find(typeId) == EggTypes.end()) {
        std::cout << "[WARN] Egg Type " << typeId << " not found!" << std::endl;
        return;
    }

    EggType* type = &EggTypes[typeId];

    // buff the player
    if (type->effectId != 0)
        eggBuffPlayer(sock, type->effectId, eggId, type->duration);

    /*
     * SHINY_PICKUP_SUCC is only causing a GUI effect in the client
     * (buff icon pops up in the bottom of the screen)
     * so we don't send it for non-effect
     */

    if (type->effectId != 0)
    {
        INITSTRUCT(sP_FE2CL_REP_SHINY_PICKUP_SUCC, resp);
        resp.iSkillID = type->effectId;

        // in general client finds correct icon on it's own,
        // but for damage we have to supply correct CSTB
        if (resp.iSkillID == 183)
            resp.eCSTB = ECSB_INFECTION;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_SHINY_PICKUP_SUCC, sizeof(sP_FE2CL_REP_SHINY_PICKUP_SUCC));
    }

    // drop
    if (type->dropCrateId != 0) {
        const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        // we know it's only one trailing struct, so we can skip full validation

        uint8_t respbuf[resplen]; // not a variable length array, don't worry
        sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
        sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

        // don't forget to zero the buffer!
        memset(respbuf, 0, resplen);

        // send back player's stats
        reward->m_iCandy = plr->money;
        reward->m_iFusionMatter = plr->fusionmatter;
        reward->m_iBatteryN = plr->batteryN;
        reward->m_iBatteryW = plr->batteryW;
        reward->iFatigue = 100; // prevents warning message
        reward->iFatigue_Level = 1;
        reward->iItemCnt = 1; // remember to update resplen if you change this

        int slot = Items::findFreeSlot(plr);

        // no space for drop
        if (slot != -1) {

            // item reward
            item->sItem.iType = 9;
            item->sItem.iOpt = 1;
            item->sItem.iID = type->dropCrateId;
            item->iSlotNum = slot;
            item->eIL = 1; // Inventory Location. 1 means player inventory.

            // update player
            plr->Inven[slot] = item->sItem;
            sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
        }
    }

    if (egg->summoned)
        destroyNPC(eggId);
    else {
        Chunking::removeNPCFromChunks(Chunking::getViewableChunks(egg->chunkPos), eggId);
        egg->dead = true;
        egg->deadUntil = getTime() + (time_t)type->regen * 1000;
        egg->appearanceData.iHP = 0;
    }
}

// TODO: Move this to MobAI, possibly
#pragma region NPCEvents

// summon right arm and stage 2 body
static void lordFuseStageTwo(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc; // adaptium, stun
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage two" << std::endl;

    // Fuse doesn't move; spawnX, etc. is shorter to write than *appearanceData*
    // Blastons, Heal
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2467);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // right arm, Adaptium, Stun
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX - 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2469);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

// summon left arm and stage 3 body
static void lordFuseStageThree(CNSocket *sock, BaseNPC *npc) {
    Mob *oldbody = (Mob*)npc;
    Player *plr = PlayerManager::getPlayer(sock);

    std::cout << "Lord Fuse stage three" << std::endl;

    // Cosmix, Damage Point
    Mob *newbody = (Mob*)NPCManager::summonNPC(oldbody->spawnX, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2468);

    newbody->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(newbody->appearanceData.iNPC_ID, newbody->spawnX, newbody->spawnY, newbody->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);

    // Blastons, Heal
    Mob *arm = (Mob*)NPCManager::summonNPC(oldbody->spawnX + 600, oldbody->spawnY, oldbody->spawnZ, plr->instanceID, 2470);

    arm->appearanceData.iAngle = oldbody->appearanceData.iAngle;
    NPCManager::updateNPCPosition(arm->appearanceData.iNPC_ID, arm->spawnX, arm->spawnY, arm->spawnZ,
        plr->instanceID, oldbody->appearanceData.iAngle);
}

std::vector<NPCEvent> NPCManager::NPCEvents = {
    NPCEvent(2466, ON_KILLED, lordFuseStageTwo),
    NPCEvent(2467, ON_KILLED, lordFuseStageThree),
};

#pragma endregion NPCEvents

void NPCManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_USE_NPC, npcWarpHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TIME_TO_GO_WARP, npcWarpTimeMachine);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_SUMMON, npcSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NPC_UNSUMMON, npcUnsummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_BARKER, npcBarkHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SHINY_PICKUP, eggPickup);

    REGISTER_SHARD_TIMER(eggStep, 1000);
}
