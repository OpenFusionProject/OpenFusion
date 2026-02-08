#include "PlayerManager.hpp"

#include "db/Database.hpp"
#include "core/CNShared.hpp"
#include "servers/CNShardServer.hpp"

#include "NPCManager.hpp"
#include "Combat.hpp"
#include "Racing.hpp"
#include "Eggs.hpp"
#include "Missions.hpp"
#include "Chat.hpp"
#include "Items.hpp"
#include "Buddies.hpp"
#include "BuiltinCommands.hpp"

#include <assert.h>
#include <algorithm>
#include <vector>
#include <cmath>

using namespace PlayerManager;

std::map<CNSocket*, Player*> PlayerManager::players;

static void addPlayer(CNSocket* key, Player *plr) {
    players[key] = plr;
    plr->chunkPos = Chunking::INVALID_CHUNK;
    plr->lastHeartbeat = 0;

    std::cout << getPlayerName(plr) << " has joined!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayer(CNSocket* key) {
    Player* plr = getPlayer(key);
    uint64_t fromInstance = plr->instanceID;

    // free buff memory
    for(auto buffEntry : plr->buffs)
        delete buffEntry.second;

    // leave group
    if(plr->group != nullptr)
        Groups::groupKick(plr->group, key);

    // remove player's bullets
    Combat::Bullets.erase(plr->iID);

    // remove player's ongoing race, if it exists
    Racing::EPRaces.erase(key);

    // save player to DB
    Database::updatePlayer(plr);

    // remove player visually and untrack
    EntityRef ref = {key};
    Chunking::removeEntityFromChunks(Chunking::getViewableChunks(plr->chunkPos), ref);
    Chunking::untrackEntity(plr->chunkPos, ref);

    std::cout << getPlayerName(plr) << " has left!" << std::endl;

    delete plr;
    players.erase(key);

    // if the player was in a lair, clean it up
    Chunking::destroyInstanceIfEmpty(fromInstance);

    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, uint64_t I, int angle) {
    Player* plr = getPlayer(sock);
    plr->angle = angle;
    ChunkPos oldChunk = plr->chunkPos;
    ChunkPos newChunk = Chunking::chunkPosAt(X, Y, I);
    plr->x = X;
    plr->y = Y;
    plr->z = Z;
    if (plr->instanceID != I) {
        plr->instanceID = I;
        plr->recallInstance = INSTANCE_OVERWORLD;
    }
    if (oldChunk == newChunk)
        return; // didn't change chunks
    Chunking::updateEntityChunk({sock}, oldChunk, newChunk);
}

/*
 * Low-level helper function for correctly updating chunks when teleporting players.
 *
 * Use PlayerManager::sendPlayerTo() to actually teleport players.
 */
void PlayerManager::updatePlayerPositionForWarp(CNSocket* sock, int X, int Y, int Z, uint64_t inst) {
    Player *plr = getPlayer(sock);

    // force player to reload chunks
    Chunking::updateEntityChunk({sock}, plr->chunkPos, Chunking::INVALID_CHUNK);
    updatePlayerPosition(sock, X, Y, Z, inst, plr->angle);
}

void PlayerManager::sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I) {
    Player* plr = getPlayer(sock);
    plr->onMonkey = false;

    if (plr->instanceID == INSTANCE_OVERWORLD) {
        // save last uninstanced coords
        plr->lastX = plr->x;
        plr->lastY = plr->y;
        plr->lastZ = plr->z;
        plr->lastAngle = plr->angle;
    }

    Missions::failInstancedMissions(sock); // fail any instanced missions

    uint64_t fromInstance = plr->instanceID; // pre-warp instance, saved for post-warp

    if (I == INSTANCE_OVERWORLD || (I != fromInstance && fromInstance != 0)) {
        // annoying but necessary to set the flag back
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp);
        resp.iX = X;
        resp.iY = Y;
        resp.iZ = Z;
        resp.iCandy = plr->money;
        resp.eIL = 4; // do not take away any items
        sock->sendPacket(resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC);
    }

    INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, pkt2);
    pkt2.iX = X;
    pkt2.iY = Y;
    pkt2.iZ = Z;
    sock->sendPacket(pkt2, P_FE2CL_REP_PC_GOTO_SUCC);

    updatePlayerPositionForWarp(sock, X, Y, Z, I);

    // post-warp: check if the source instance has no more players in it and delete it if so
    Chunking::destroyInstanceIfEmpty(fromInstance);

    // clean up EPRaces if we were likely in an IZ and left
    if (fromInstance != INSTANCE_OVERWORLD && fromInstance != I
    && Racing::EPRaces.find(sock) != Racing::EPRaces.end())
        Racing::EPRaces.erase(sock);
}

void PlayerManager::sendPlayerTo(CNSocket* sock, int X, int Y, int Z) {
    sendPlayerTo(sock, X, Y, Z, getPlayer(sock)->instanceID);
}

/*
 * Sends all nanos, from 0 to 58 (the contents of the Nanos array in PC_ENTER_SUCC are totally irrelevant).
 * The first Nano in the in-game nanobook is the Unstable Nano, which is Van Kleiss.
 * 0 (in plr->Nanos) is the null nano entry.
 * 58 is a "Coming Soon" duplicate entry for an actual Van Kleiss nano, identical to the Unstable Nano.
 * Nanos the player hasn't unlocked will (and should) be greyed out. Thus, all nanos should be accounted
 * for in these packets, even if the player hasn't unlocked them.
 */
static void sendNanoBook(CNSocket *sock, Player *plr, bool resizeOnly) {
#ifdef ACADEMY
    int16_t id = 0;
    INITSTRUCT(sP_FE2CL_REP_NANO_BOOK_SUBSET, pkt);

    pkt.PCUID = plr->iID;
    pkt.bookSize = NANO_COUNT;

    if (resizeOnly) {
        // triggers nano array resizing without
        // actually sending nanos
        sock->sendPacket(pkt, P_FE2CL_REP_NANO_BOOK_SUBSET);
        return;
    }

    while (id < NANO_COUNT) {
        pkt.elementOffset = id;

        for (int i = id - pkt.elementOffset; id < NANO_COUNT && i < 10; id++, i = id - pkt.elementOffset)
            pkt.element[i] = plr->Nanos[id];

        sock->sendPacket(pkt, P_FE2CL_REP_NANO_BOOK_SUBSET);
    }
#endif
}

static void enterPlayer(CNSocket* sock, CNPacketData* data) {
    auto enter = (sP_CL2FE_REQ_PC_ENTER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ENTER_SUCC, response);

    LoginMetadata *lm = CNShared::getLoginMetadata(enter->iEnterSerialKey);
    if (lm == nullptr) {
        std::cout << "[WARN] Refusing invalid REQ_PC_ENTER" << std::endl;

        // send failure packet
        INITSTRUCT(sP_FE2CL_REP_PC_ENTER_FAIL, fail);
        sock->sendPacket(fail, P_FE2CL_REP_PC_ENTER_FAIL);

        // kill the connection
        sock->kill();
        // no need to call _killConnection(); Player isn't in shard yet

        return;
    }

    Player *plr = new Player();
    Database::getPlayer(plr, lm->playerId);

    // check if account is already in use
    if (isAccountInUse(plr->accountId)) {
        // kick the other player
        exitDuplicate(plr->accountId);

        // re-read the player from disk, in case it was just flushed
        *plr = {};
        Database::getPlayer(plr, lm->playerId);
    }

    plr->group = nullptr;

    response.iID = plr->iID;
    response.uiSvrTime = getTime();

    response.PCLoadData2CL.iUserLevel = plr->accountLevel;
    response.PCLoadData2CL.iHP = plr->HP;
    response.PCLoadData2CL.iLevel = plr->level;
    response.PCLoadData2CL.iCandy = plr->money;
    response.PCLoadData2CL.iFusionMatter = plr->fusionmatter;
    response.PCLoadData2CL.iMentor = plr->mentor;
    response.PCLoadData2CL.iMentorCount = 1; // how many guides the player has had
    response.PCLoadData2CL.iX = plr->x;
    response.PCLoadData2CL.iY = plr->y;
    response.PCLoadData2CL.iZ = plr->z;
    response.PCLoadData2CL.iAngle = plr->angle;
    response.PCLoadData2CL.iBatteryN = plr->batteryN;
    response.PCLoadData2CL.iBatteryW = plr->batteryW;
    response.PCLoadData2CL.iBuddyWarpTime = 60; // sets 60s warp cooldown on login

    response.PCLoadData2CL.iWarpLocationFlag = plr->iWarpLocationFlag;
    response.PCLoadData2CL.aWyvernLocationFlag[0] = plr->aSkywayLocationFlag[0];
    response.PCLoadData2CL.aWyvernLocationFlag[1] = plr->aSkywayLocationFlag[1];

    response.PCLoadData2CL.iActiveNanoSlotNum = -1;
    response.PCLoadData2CL.iFatigue = 50;
    response.PCLoadData2CL.PCStyle = plr->PCStyle;

    // client doesnt read this, it gets it from charinfo
    // response.PCLoadData2CL.PCStyle2 = plr->PCStyle2;

    // equipment (except nanocom boosters)
    for (int i = 0; i < 9; i++)
        response.PCLoadData2CL.aEquip[i] = plr->Equip[i];
    // equipment (nanocom boosters)
    int32_t serverTime = getTime() / 1000UL;
    int32_t timestamp = getTimestamp();
    for (int i = 9; i < AEQUIP_COUNT; i++) {
        response.PCLoadData2CL.aEquip[i] = plr->Equip[i];
        // client subtracts server time, then adds local timestamp to the item to print expiration time
        response.PCLoadData2CL.aEquip[i].iTimeLimit = std::max(0, plr->Equip[i].iTimeLimit - timestamp + serverTime);
    }
    // inventory
    for (int i = 0; i < AINVEN_COUNT; i++)
        response.PCLoadData2CL.aInven[i] = plr->Inven[i];
    // quest inventory
    for (int i = 0; i < AQINVEN_COUNT; i++)
        response.PCLoadData2CL.aQInven[i] = plr->QInven[i];
    // nanos
    for (int i = 1; i < SIZEOF_NANO_BANK_SLOT; i++) {
        response.PCLoadData2CL.aNanoBank[i] = plr->Nanos[i];
    }
    for (int i = 0; i < 3; i++) {
        response.PCLoadData2CL.aNanoSlots[i] = plr->equippedNanos[i];
    }
    // missions in progress
    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] == 0)
            break;
        response.PCLoadData2CL.aRunningQuest[i].m_aCurrTaskID = plr->tasks[i];
        TaskData &task = *Missions::Tasks[plr->tasks[i]];
        for (int j = 0; j < 3; j++) {
            response.PCLoadData2CL.aRunningQuest[i].m_aKillNPCID[j] = (int)task["m_iCSUEnemyID"][j];
            response.PCLoadData2CL.aRunningQuest[i].m_aKillNPCCount[j] = plr->RemainingNPCCount[i][j];
            /*
             * client doesn't care about NeededItem ID and Count,
             * it gets Count from Quest Inventory
             *
             * KillNPCCount sets RemainEnemyNum in the client
             * Yes, this is extraordinary stupid.
            */
        }
    }
    response.PCLoadData2CL.iCurrentMissionID = plr->CurrentMissionID;

    // completed missions
    // the packet requires 32 items, but the client only checks the first 16 (shrug)
    for (int i = 0; i < 16; i++) {
        response.PCLoadData2CL.aQuestFlag[i] = plr->aQuestFlag[i];
    }

    // Computress tips
    if (settings::DISABLEFIRSTUSEFLAG) {
        response.PCLoadData2CL.iFirstUseFlag1 = UINT64_MAX;
        response.PCLoadData2CL.iFirstUseFlag2 = UINT64_MAX;
    }
    else {
        response.PCLoadData2CL.iFirstUseFlag1 = plr->iFirstUseFlag[0];
        response.PCLoadData2CL.iFirstUseFlag2 = plr->iFirstUseFlag[1];
    }

    plr->instanceID = INSTANCE_OVERWORLD; // the player should never be in an instance on enter

    sock->setEKey(CNSocketEncryption::createNewKey(response.uiSvrTime, response.iID + 1, response.PCLoadData2CL.iFusionMatter + 1));
    sock->setFEKey(lm->FEKey);
    sock->setActiveKey(SOCKETKEY_FE); // send all packets using the FE key from now on

    // Academy builds receive nanos in a separate packet. An initial one with the size of the
    // nano book needs to be sent before PC_ENTER_SUCC so the client can resize its nano arrays,
    // and then proper packets with the nanos included must be sent after, while the game is loading.
    sendNanoBook(sock, plr, true);

    sock->sendPacket(response, P_FE2CL_REP_PC_ENTER_SUCC);

    sendNanoBook(sock, plr, false);

    // transfer ownership of Player object into the shard (still valid in this function though)
    addPlayer(sock, plr);

    // set player equip stats
    Items::setItemStats(plr);

    for (auto& pair : players)
        if (pair.second->notify)
            Chat::sendServerMessage(pair.first, "[ADMIN]" + getPlayerName(plr) + " has joined.");

    // deallocate lm
    delete lm;
}

void PlayerManager::sendToGroup(CNSocket* sock, void* buf, uint32_t type, size_t size) {
    Player* plr = getPlayer(sock);
    if (plr->group == nullptr)
        return;
    for(const EntityRef& ref : plr->group->filter(EntityKind::PLAYER))
        ref.sock->sendPacket(buf, type, size);
}

void PlayerManager::sendToViewable(CNSocket* sock, void* buf, uint32_t type, size_t size) {
    Player* plr = getPlayer(sock);
    for (auto it = plr->viewableChunks.begin(); it != plr->viewableChunks.end(); it++) {
        Chunk* chunk = *it;
        for (const EntityRef& ref : chunk->entities) {
            if (ref.kind != EntityKind::PLAYER || ref.sock == sock)
                continue;

            ref.sock->sendPacket(buf, type, size);
        }
    }
}

static void loadPlayer(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_LOADING_COMPLETE* complete = (sP_CL2FE_REQ_PC_LOADING_COMPLETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, response);
    Player *plr = getPlayer(sock);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_LOADING_COMPLETE:" << std::endl;
        std::cout << "\tPC_ID: " << complete->iPC_ID << std::endl;
    )

    response.iPC_ID = complete->iPC_ID;

    updatePlayerPosition(sock, plr->x, plr->y, plr->z, plr->instanceID, plr->angle);

    sock->sendPacket(response, P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC);

    if (plr->instanceID != INSTANCE_OVERWORLD) {
        INITSTRUCT(sP_FE2CL_INSTANCE_MAP_INFO, pkt);
        pkt.iInstanceMapNum = (int32_t)MAPNUM(plr->instanceID); // lower 32 bits are mapnum
        if (pkt.iInstanceMapNum != plr->recallInstance // do not retransmit MAP_INFO on recall
        && Racing::EPData.find(pkt.iInstanceMapNum) != Racing::EPData.end()) {
            EPInfo* ep = &Racing::EPData[pkt.iInstanceMapNum];
            pkt.iEP_ID = ep->EPID;
            pkt.iMapCoordX_Min = ep->zoneX * 51200;
            pkt.iMapCoordX_Max = (ep->zoneX + 1) * 51200;
            pkt.iMapCoordY_Min = ep->zoneY * 51200;
            pkt.iMapCoordY_Max = (ep->zoneY + 1) * 51200;
            pkt.iMapCoordZ_Min = INT32_MIN;
            pkt.iMapCoordZ_Max = INT32_MAX;
        }

        sock->sendPacket(pkt, P_FE2CL_INSTANCE_MAP_INFO);
    }

    if (!plr->initialLoadDone) {
        // these should be called only once, but not until after
        // first load-in or else the client may ignore the packets
        Chat::sendServerMessage(sock, settings::MOTDSTRING); // MOTD
        Missions::failInstancedMissions(sock); // auto-fail missions
        Buddies::sendBuddyList(sock); // buddy list
        Items::checkAndRemoveExpiredItems(sock, plr); // vehicle and booster expiration

        plr->initialLoadDone = true;
    }
}

static void heartbeatPlayer(CNSocket* sock, CNPacketData* data) {
    getPlayer(sock)->lastHeartbeat = getTime();
}

static void exitGame(CNSocket* sock, CNPacketData* data) {
    auto exitData = (sP_CL2FE_REQ_PC_EXIT*)data->buf;

    // Refresh any auth cookie, in case "change character" was used
    Player* plr = getPlayer(sock);
    if (plr != nullptr) {
        // 5 seconds should be enough to log in again
        Database::refreshCookie(plr->accountId, 5);
    }

    INITSTRUCT(sP_FE2CL_REP_PC_EXIT_SUCC, response);

    response.iID = exitData->iID;
    response.iExitCode = 1;

    sock->sendPacket(response, P_FE2CL_REP_PC_EXIT_SUCC);

    sock->kill();
    CNShardServer::_killConnection(sock);
}

static void revivePlayer(CNSocket* sock, CNPacketData* data) {
    Player *plr = getPlayer(sock);
    WarpLocation* target = getRespawnPoint(plr);

    auto reviveData = (sP_CL2FE_REQ_PC_REGEN*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_REGEN_SUCC, response);
    INITSTRUCT(sP_FE2CL_PC_REGEN, resp2);

    int activeSlot = -1;
    bool move = false;

    switch ((ePCRegenType)reviveData->iRegenType) {
    case ePCRegenType::HereByPhoenix: // nano revive
        if (!(plr->hasBuff(ECSB_PHOENIX)))
            return; // sanity check
        plr->Nanos[plr->activeNano].iStamina = 0;
        // fallthrough
    case ePCRegenType::HereByPhoenixGroup: // revived by group member's nano
        plr->HP = PC_MAXHEALTH(plr->level) / 2;
        break;

    default: // plain respawn
        plr->HP = PC_MAXHEALTH(plr->level) / 2;
        plr->clearBuffs(false);
        // fallthrough
    case ePCRegenType::Unstick: // warp away
        move = true;
        break;
    }

    for (int i = 0; i < 3; i++) {
        int nanoID = plr->equippedNanos[i];
        // halve nano health if respawning
        // all revives not 3-5 are normal respawns.
        if (reviveData->iRegenType < 3 || reviveData->iRegenType > 5)
            plr->Nanos[nanoID].iStamina = 75; // max is 150, so 75 is half
        response.PCRegenData.Nanos[i] = plr->Nanos[nanoID];
        if (plr->activeNano == nanoID)
            activeSlot = i;
    }

    int x, y, z;
    if (move && target != nullptr) {
        // go to Resurrect 'Em
        x = target->x;
        y = target->y;
        z = target->z;
    } else if (PLAYERID(plr->instanceID)) {
        // respawn at entrance to the Lair
        x = plr->recallX;
        y = plr->recallY;
        z = plr->recallZ;
    } else {
        // no other choice; respawn in place
        x = plr->x;
        y = plr->y;
        z = plr->z;
    }

    // Response parameters
    response.PCRegenData.iActiveNanoSlotNum = activeSlot;
    response.PCRegenData.iX = x;
    response.PCRegenData.iY = y;
    response.PCRegenData.iZ = z;
    response.PCRegenData.iHP = plr->HP;
    response.iFusionMatter = plr->fusionmatter;
    response.bMoveLocation = 0;
    response.PCRegenData.iMapNum = MAPNUM(plr->instanceID);

    sock->sendPacket(response, P_FE2CL_REP_PC_REGEN_SUCC);

    // Update other players
    resp2.PCRegenDataForOtherPC.iPC_ID = plr->iID;
    resp2.PCRegenDataForOtherPC.iX = x;
    resp2.PCRegenDataForOtherPC.iY = y;
    resp2.PCRegenDataForOtherPC.iZ = z;
    resp2.PCRegenDataForOtherPC.iHP = plr->HP;
    resp2.PCRegenDataForOtherPC.iAngle = plr->angle;

    if (plr->group != nullptr) {
        resp2.PCRegenDataForOtherPC.iConditionBitFlag = plr->getCompositeCondition();
        resp2.PCRegenDataForOtherPC.iPCState = plr->iPCState;
        resp2.PCRegenDataForOtherPC.iSpecialState = plr->iSpecialState;
        resp2.PCRegenDataForOtherPC.Nano = plr->Nanos[plr->activeNano];

        sendToViewable(sock, resp2, P_FE2CL_PC_REGEN);
    }

    if (!move)
        return;

    updatePlayerPositionForWarp(sock, x, y, z, plr->instanceID);
}

static void enterPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    Player* plr = getPlayer(sock);

    // vehicles are only allowed in the overworld
    if (plr->instanceID != 0)
        return;

    if (plr->Equip[8].iID > 0) {
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_ON_SUCC, response);
        sock->sendPacket(response, P_FE2CL_PC_VEHICLE_ON_SUCC);

        // send to other players
        plr->iPCState |= 8;
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, response2);
        response2.iPC_ID = plr->iID;
        response2.iState = plr->iPCState;
        sendToViewable(sock, response2, P_FE2CL_PC_STATE_CHANGE);

    } else {
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_ON_FAIL, response);
        sock->sendPacket(response, P_FE2CL_PC_VEHICLE_ON_FAIL);
    }
}

static void setSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data) {
    BuiltinCommands::setSpecialState(sock, data);
}

static void changePlayerGuide(CNSocket *sock, CNPacketData *data) {
    auto pkt = (sP_CL2FE_REQ_PC_CHANGE_MENTOR*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC, resp);
    Player *plr = getPlayer(sock);

    resp.iMentor = pkt->iMentor;
    resp.iMentorCnt = 1;
    resp.iFusionMatter = plr->fusionmatter; // no cost

    sock->sendPacket(resp, P_FE2CL_REP_PC_CHANGE_MENTOR_SUCC);
    // if it's changed from computress
    if (plr->mentor == 5) {
        // we're warping to the past
        plr->PCStyle2.iPayzoneFlag = 1;
        // remove all active missions
        for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
            if (plr->tasks[i] != 0)
                Missions::quitTask(sock, plr->tasks[i], true);
        }

        // start Blossom nano mission if applicable
        Missions::updateFusionMatter(sock, 0);
    }
    // save it on player
    plr->mentor = pkt->iMentor;
}

static void setFirstUseFlag(CNSocket* sock, CNPacketData* data) {
    auto flag = (sP_CL2FE_REQ_PC_FIRST_USE_FLAG_SET*)data->buf;
    Player* plr = getPlayer(sock);

    if (flag->iFlagCode < 1 || flag->iFlagCode > 128) {
        std::cout << "[WARN] Client submitted invalid first use flag number?!" << std::endl;
        return;
    }

    if (flag->iFlagCode <= 64)
        plr->iFirstUseFlag[0] |= (1ULL << (flag->iFlagCode - 1));
    else
        plr->iFirstUseFlag[1] |= (1ULL << (flag->iFlagCode - 65));
}

#pragma region Helper methods
void PlayerManager::exitPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    Player* plr = getPlayer(sock);

    if (plr->iPCState & 8) {
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, response);
        sock->sendPacket(response, P_FE2CL_PC_VEHICLE_OFF_SUCC);

        // send to other players
        plr->iPCState &= ~8;
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, response2);
        response2.iPC_ID = plr->iID;
        response2.iState = plr->iPCState;

        sendToViewable(sock, response2, P_FE2CL_PC_STATE_CHANGE);
    }
}

Player *PlayerManager::getPlayer(CNSocket* key) {
    if (players.find(key) != players.end())
        return players[key];

    // this should never happen
    assert(false);
}

std::string PlayerManager::getPlayerName(Player *plr, bool id) {
    // the print in CNShardServer can print packets from players that haven't yet joined
    if (plr == nullptr)
        return "NOT IN GAME";

    if (plr->PCStyle.iNameCheck != 1) {
        return "Player " + std::to_string(plr->iID);
    }

    std::string ret = "";
    if (id && plr->accountLevel <= 30)
        ret += "(GM) ";

    ret += AUTOU16TOU8(plr->PCStyle.szFirstName) + " " + AUTOU16TOU8(plr->PCStyle.szLastName);

    if (id)
        ret += " [" + std::to_string(plr->iID) + "]";

    return ret;
}

bool PlayerManager::isAccountInUse(int accountId) {
    std::map<CNSocket*, Player*>::iterator it;
    for (it = players.begin(); it != players.end(); it++) {
        if (it->second->accountId == accountId)
            return true;
    }
    return false;
}

void PlayerManager::exitDuplicate(int accountId) {
    std::map<CNSocket*, Player*>::iterator it;

    // disconnect any duplicate players
    for (it = players.begin(); it != players.end(); it++) {
        if (it->second->accountId == accountId) {
            CNSocket* sock = it->first;

            INITSTRUCT(sP_FE2CL_REP_PC_EXIT_DUPLICATE, resp);
            resp.iErrorCode = 0;
            sock->sendPacket(resp, P_FE2CL_REP_PC_EXIT_DUPLICATE);

            sock->kill();
            CNShardServer::_killConnection(sock);
            break;
        }
    }
}

// TODO: just call getPlayer() after getSockFromID()?
Player *PlayerManager::getPlayerFromID(int32_t iID) {
    for (auto& pair : players)
        if (pair.second->iID == iID)
            return pair.second;

    return nullptr;
}

CNSocket *PlayerManager::getSockFromID(int32_t iID) {
    for (auto& pair : players)
        if (pair.second->iID == iID)
            return pair.first;

    return nullptr;
}

CNSocket *PlayerManager::getSockFromName(std::string firstname, std::string lastname) {
    for (auto& pair : players)
        if (AUTOU16TOU8(pair.second->PCStyle.szFirstName) == firstname
        && AUTOU16TOU8(pair.second->PCStyle.szLastName) == lastname)
            return pair.first;

    return nullptr;
}

CNSocket *PlayerManager::getSockFromAny(int by, int id, int uid, std::string firstname, std::string lastname) {
    switch ((eCN_GM_TargetSearchBy)by) {
    case eCN_GM_TargetSearchBy::PC_ID:
        assert(id != 0);
        return getSockFromID(id);
    case eCN_GM_TargetSearchBy::PC_UID: // account id; not player id
        assert(uid != 0);
        for (auto& pair : players)
            if (pair.second->accountId == uid)
                return pair.first;
    case eCN_GM_TargetSearchBy::PC_Name:
        assert(firstname != "" && lastname != ""); // XXX: remove this if we start messing around with edited names?
        return getSockFromName(firstname, lastname);
    }

    // not found
    return nullptr;
}

WarpLocation *PlayerManager::getRespawnPoint(Player *plr) {
    WarpLocation* best = nullptr;
    uint32_t curDist, bestDist = UINT32_MAX;

    for (auto& targ : NPCManager::RespawnPoints) {
        curDist = sqrt(pow(plr->x - targ.x, 2) + pow(plr->y - targ.y, 2));
        if (curDist < bestDist && targ.instanceID == MAPNUM(plr->instanceID)) { // only mapNum needs to match
            best = &targ;
            bestDist = curDist;
        }
    }

    return best;
}

#pragma endregion

void PlayerManager::init() {
    // register packet types
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ENTER, enterPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_LOADING_COMPLETE, loadPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REP_LIVE_CHECK, heartbeatPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_REGEN, revivePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EXIT, exitGame);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH, setSpecialSwitchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_ON, enterPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_OFF, exitPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_CHANGE_MENTOR, changePlayerGuide);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_FIRST_USE_FLAG_SET, setFirstUseFlag);

    REGISTER_SHARD_TIMER(CNShared::pruneLoginMetadata, CNSHARED_PERIOD);
}
