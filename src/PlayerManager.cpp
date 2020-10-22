#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "CNShardServer.hpp"
#include "CNShared.hpp"
#include "MissionManager.hpp"
#include "ItemManager.hpp"
#include "NanoManager.hpp"
#include "GroupManager.hpp"
#include "ChatManager.hpp"
#include "Database.hpp"

#include "settings.hpp"

#include <assert.h>

#include <algorithm>
#include <vector>
#include <cmath>

std::map<CNSocket*, PlayerView> PlayerManager::players;

void PlayerManager::init() {
    // register packet types
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ENTER, PlayerManager::enterPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_LOADING_COMPLETE, PlayerManager::loadPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVE, PlayerManager::movePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_STOP, PlayerManager::stopPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_JUMP, PlayerManager::jumpPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_JUMPPAD, PlayerManager::jumppadPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_LAUNCHER, PlayerManager::launchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ZIPLINE, PlayerManager::ziplinePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVEPLATFORM, PlayerManager::movePlatformPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVETRANSPORTATION, PlayerManager::moveSliderPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SLOPE, PlayerManager::moveSlopePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GOTO, PlayerManager::gotoPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SET_VALUE, PlayerManager::setSpecialPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REP_LIVE_CHECK, PlayerManager::heartbeatPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_REGEN, PlayerManager::revivePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EXIT, PlayerManager::exitGame);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH, PlayerManager::setSpecialSwitchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH, PlayerManager::setGMSpecialSwitchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_ON, PlayerManager::enterPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_OFF, PlayerManager::exitPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_CHANGE_MENTOR, PlayerManager::changePlayerGuide);
}

void PlayerManager::addPlayer(CNSocket* key, Player plr) {
    Player *p = new Player();

    memcpy(p, &plr, sizeof(Player));

    players[key] = PlayerView();
    players[key].chunkPos = std::make_tuple(0, 0, 0);
    players[key].currentChunks = std::vector<Chunk*>();
    players[key].plr = p;
    players[key].lastHeartbeat = 0;

    key->plr = p;

    std::cout << U16toU8(plr.PCStyle.szFirstName) << " " << U16toU8(plr.PCStyle.szLastName) << " has joined!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayer(CNSocket* key) {
    PlayerView& view = players[key];
    uint64_t fromInstance = view.plr->instanceID;

    GroupManager::groupKickPlayer(view.plr);

    // save player to DB
    Database::updatePlayer(view.plr);

    // remove players from all chunks
    removePlayerFromChunks(view.currentChunks, key);

    // remove from chunk
    if (ChunkManager::chunks.find(view.chunkPos) != ChunkManager::chunks.end())
        ChunkManager::chunks[view.chunkPos]->players.erase(key);

    std::cout << U16toU8(view.plr->PCStyle.szFirstName) << " " << U16toU8(view.plr->PCStyle.szLastName) << " (PlayerId = " << view.plr->iID << ") has left!" << std::endl;

    key->plr = nullptr;
    delete view.plr;
    players.erase(key);

    // if the player was in a lair, clean it up
    ChunkManager::destroyInstanceIfEmpty(fromInstance);

    std::cout << players.size() << " players" << std::endl;
}

bool PlayerManager::removePlayerFromChunks(std::vector<Chunk*> chunks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_PC_EXIT, exitPlayer);

    // for chunks that need the player to be removed from
    for (Chunk* chunk : chunks) {

        // remove NPCs
        for (int32_t id : chunk->NPCs) {
            BaseNPC* npc = NPCManager::NPCs[id];
            switch (npc->npcClass) {
            case NPC_BUS:
                INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, exitBusData);
                exitBusData.eTT = 3;
                exitBusData.iT_ID = id;
                sock->sendPacket((void*)&exitBusData, P_FE2CL_TRANSPORTATION_EXIT, sizeof(sP_FE2CL_TRANSPORTATION_EXIT));
                break;
            default:
                INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
                exitData.iNPC_ID = id;
                sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
                break;
            }
        }

        // remove players from eachother
        for (CNSocket* otherSock : chunk->players) {
            exitPlayer.iID = players[sock].plr->iID;
            otherSock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
            exitPlayer.iID = players[otherSock].plr->iID;
            sock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
        }
    }

    // remove us from that old stinky chunk
    return ChunkManager::removePlayer(players[sock].chunkPos, sock);
}

void PlayerManager::addPlayerToChunks(std::vector<Chunk*> chunks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_PC_NEW, newPlayer);

    for (Chunk* chunk : chunks) {
        // add npcs
        for (int32_t id : chunk->NPCs) {
            BaseNPC* npc = NPCManager::NPCs[id];
            switch (npc->npcClass) {
            case NPC_BUS:
                INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, enterBusData);
                enterBusData.AppearanceData = { 3, npc->appearanceData.iNPC_ID, npc->appearanceData.iNPCType, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ };
                sock->sendPacket((void*)&enterBusData, P_FE2CL_TRANSPORTATION_ENTER, sizeof(sP_FE2CL_TRANSPORTATION_ENTER));
                break;
            default:
                INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
                enterData.NPCAppearanceData = NPCManager::NPCs[id]->appearanceData;
                sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
                break;
            }
        }

        // add players
        for (CNSocket* otherSock : chunk->players) {
            Player *otherPlr = players[otherSock].plr;
            Player *plr = players[sock].plr;

            newPlayer.PCAppearanceData.iID = plr->iID;
            newPlayer.PCAppearanceData.iHP = plr->HP;
            newPlayer.PCAppearanceData.iLv = plr->level;
            newPlayer.PCAppearanceData.iX = plr->x;
            newPlayer.PCAppearanceData.iY = plr->y;
            newPlayer.PCAppearanceData.iZ = plr->z;
            newPlayer.PCAppearanceData.iAngle = plr->angle;
            newPlayer.PCAppearanceData.PCStyle = plr->PCStyle;
            newPlayer.PCAppearanceData.Nano = plr->Nanos[plr->activeNano];
            newPlayer.PCAppearanceData.iPCState = plr->iPCState;
            newPlayer.PCAppearanceData.iSpecialState = plr->iSpecialState;
            memcpy(newPlayer.PCAppearanceData.ItemEquip, plr->Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            otherSock->sendPacket((void*)&newPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW));

            newPlayer.PCAppearanceData.iID = otherPlr->iID;
            newPlayer.PCAppearanceData.iHP = otherPlr->HP;
            newPlayer.PCAppearanceData.iLv = otherPlr->level;
            newPlayer.PCAppearanceData.iX = otherPlr->x;
            newPlayer.PCAppearanceData.iY = otherPlr->y;
            newPlayer.PCAppearanceData.iZ = otherPlr->z;
            newPlayer.PCAppearanceData.iAngle = otherPlr->angle;
            newPlayer.PCAppearanceData.PCStyle = otherPlr->PCStyle;
            newPlayer.PCAppearanceData.Nano = otherPlr->Nanos[otherPlr->activeNano];
            newPlayer.PCAppearanceData.iPCState = otherPlr->iPCState;
            newPlayer.PCAppearanceData.iSpecialState = otherPlr->iSpecialState;
            memcpy(newPlayer.PCAppearanceData.ItemEquip, otherPlr->Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            sock->sendPacket((void*)&newPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW));
        }
    }
}

void PlayerManager::updatePlayerPosition(CNSocket* sock, int X, int Y, int Z, int angle) {
    players[sock].plr->angle = angle;
    updatePlayerPosition(sock, X, Y, Z);
}

void PlayerManager::updatePlayerPosition(CNSocket* sock, int X, int Y, int Z) {
    PlayerView& view = players[sock];
    view.plr->x = X;
    view.plr->y = Y;
    view.plr->z = Z;
    updatePlayerChunk(sock, X, Y, view.plr->instanceID);
}

void PlayerManager::updatePlayerChunk(CNSocket* sock, int X, int Y, uint64_t instanceID) {
    PlayerView& view = players[sock];
    std::tuple<int, int, uint64_t> newPos = ChunkManager::grabChunk(X, Y, view.plr->instanceID);

    // nothing to be done
    if (newPos == view.chunkPos)
        return;

    // add player to chunk
    std::vector<Chunk*> allChunks = ChunkManager::grabChunks(newPos);

    Chunk *chunk = nullptr;
    if (ChunkManager::checkChunk(view.chunkPos))
        chunk = ChunkManager::chunks[view.chunkPos];

    // first, remove all the old npcs & players from the old chunks
    if (removePlayerFromChunks(ChunkManager::getDeltaChunks(view.currentChunks, allChunks), sock)) {
        allChunks.erase(std::remove(allChunks.begin(), allChunks.end(), chunk), allChunks.end());
    }

    // now, add all the new npcs & players!
    addPlayerToChunks(ChunkManager::getDeltaChunks(allChunks, view.currentChunks), sock);

    ChunkManager::addPlayer(X, Y, view.plr->instanceID, sock); // takes care of adding the player to the chunk if it exists or not
    view.chunkPos = newPos;
    view.currentChunks = allChunks;
}

void PlayerManager::sendPlayerTo(CNSocket* sock, int X, int Y, int Z, uint64_t I) {
    PlayerView& plrv = PlayerManager::players[sock];
    Player* plr = plrv.plr;

    if (plrv.plr->instanceID == 0) {
        // save last uninstanced coords
        plrv.plr->lastX = plrv.plr->x;
        plrv.plr->lastY = plrv.plr->y;
        plrv.plr->lastZ = plrv.plr->z;
        plrv.plr->lastAngle = plrv.plr->angle;
    }

    MissionManager::failInstancedMissions(sock); // fail any instanced missions

    uint64_t fromInstance = plr->instanceID;

    plr->instanceID = I;
    if (I != INSTANCE_OVERWORLD) {
        INITSTRUCT(sP_FE2CL_INSTANCE_MAP_INFO, pkt);
        pkt.iEP_ID = PLAYERID(I) == 0; // iEP_ID has to be positive for the map to be enabled
        pkt.iInstanceMapNum = (int32_t)MAPNUM(I); // lower 32 bits are mapnum
        sock->sendPacket((void*)&pkt, P_FE2CL_INSTANCE_MAP_INFO, sizeof(sP_FE2CL_INSTANCE_MAP_INFO));
        sendPlayerTo(sock, X, Y, Z);
    } else {
        // annoying but necessary to set the flag back
        INITSTRUCT(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC, resp);
        resp.iX = X;
        resp.iY = Y;
        resp.iZ = Z;
        resp.iCandy = plrv.plr->money;
        resp.eIL = 4; // do not take away any items
        PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
        plrv.currentChunks.clear();
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_WARP_USE_NPC_SUCC, sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC));
    }

    ChunkManager::destroyInstanceIfEmpty(fromInstance);
}

void PlayerManager::sendPlayerTo(CNSocket* sock, int X, int Y, int Z) {
    PlayerManager::updatePlayerPosition(sock, X, Y, Z);
    INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, pkt);
    pkt.iX = X;
    pkt.iY = Y;
    pkt.iZ = Z;

    // force player & NPC reload
    PlayerView& plrv = players[sock];
    PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
    plrv.currentChunks.clear();
    plrv.chunkPos = std::make_tuple(0, 0, plrv.plr->instanceID);
    sock->sendPacket((void*)&pkt, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));
}

void PlayerManager::enterPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ENTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ENTER* enter = (sP_CL2FE_REQ_PC_ENTER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ENTER_SUCC, response);

    // TODO: check if serialkey exists, if it doesn't send sP_FE2CL_REP_PC_ENTER_FAIL
    Player plr = CNSharedData::getPlayer(enter->iEnterSerialKey);

    plr.groupCnt = 1;
    plr.iIDGroup = plr.groupIDs[0] = plr.iID;

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_ENTER:" << std::endl;
        std::cout << "\tID: " << U16toU8(enter->szID) << std::endl;
        std::cout << "\tSerial: " << enter->iEnterSerialKey << std::endl;
        std::cout << "\tTemp: " << enter->iTempValue << std::endl;
        std::cout << "\tPC_UID: " << plr.PCStyle.iPC_UID << std::endl;
    )

    // check if account is already in use
    if (isAccountInUse(plr.accountId)) {
        // kick the other player
        exitDuplicate(plr.accountId);
    }

    response.iID = plr.iID;
    response.uiSvrTime = getTime();
    response.PCLoadData2CL.iUserLevel = plr.accountLevel;
    response.PCLoadData2CL.iHP = plr.HP;
    response.PCLoadData2CL.iLevel = plr.level;
    response.PCLoadData2CL.iCandy = plr.money;
    response.PCLoadData2CL.iFusionMatter = plr.fusionmatter;
    response.PCLoadData2CL.iMentor = plr.mentor;
    response.PCLoadData2CL.iMentorCount = 1; // how many guides the player has had
    response.PCLoadData2CL.iX = plr.x;
    response.PCLoadData2CL.iY = plr.y;
    response.PCLoadData2CL.iZ = plr.z;
    response.PCLoadData2CL.iAngle = plr.angle;
    response.PCLoadData2CL.iBatteryN = plr.batteryN;
    response.PCLoadData2CL.iBatteryW = plr.batteryW;
    response.PCLoadData2CL.iBuddyWarpTime = 60; // sets 60s warp cooldown on login

    response.PCLoadData2CL.iWarpLocationFlag = plr.iWarpLocationFlag;
    response.PCLoadData2CL.aWyvernLocationFlag[0] = plr.aSkywayLocationFlag[0];
    response.PCLoadData2CL.aWyvernLocationFlag[1] = plr.aSkywayLocationFlag[1];

    response.PCLoadData2CL.iActiveNanoSlotNum = -1;
    response.PCLoadData2CL.iFatigue = 50;
    response.PCLoadData2CL.PCStyle = plr.PCStyle;

    // client doesnt read this, it gets it from charinfo
    // response.PCLoadData2CL.PCStyle2 = plr.PCStyle2;
    // inventory
    for (int i = 0; i < AEQUIP_COUNT; i++)
        response.PCLoadData2CL.aEquip[i] = plr.Equip[i];
    for (int i = 0; i < AINVEN_COUNT; i++)
        response.PCLoadData2CL.aInven[i] = plr.Inven[i];
    // quest inventory
    for (int i = 0; i < AQINVEN_COUNT; i++)
        response.PCLoadData2CL.aQInven[i] = plr.QInven[i];
    // nanos
    for (int i = 1; i < SIZEOF_NANO_BANK_SLOT; i++) {
        response.PCLoadData2CL.aNanoBank[i] = plr.Nanos[i];
    }
    for (int i = 0; i < 3; i++) {
        response.PCLoadData2CL.aNanoSlots[i] = plr.equippedNanos[i];
    }
    // missions in progress
    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr.tasks[i] == 0)
            break;
        response.PCLoadData2CL.aRunningQuest[i].m_aCurrTaskID = plr.tasks[i];
        TaskData &task = *MissionManager::Tasks[plr.tasks[i]];
        for (int j = 0; j < 3; j++) {
            response.PCLoadData2CL.aRunningQuest[i].m_aKillNPCID[j] = (int)task["m_iCSUEnemyID"][j];
            response.PCLoadData2CL.aRunningQuest[i].m_aKillNPCCount[j] = plr.RemainingNPCCount[i][j];
            /*
             * client doesn't care about NeededItem ID and Count,
             * it gets Count from Quest Inventory
             *
             * KillNPCCount sets RemainEnemyNum in the client
             * Yes, this is extraordinary stupid.
            */
        }
    }
    response.PCLoadData2CL.iCurrentMissionID = plr.CurrentMissionID;

    // completed missions
    // the packet requires 32 items, but the client only checks the first 16 (shrug)
    for (int i = 0; i < 16; i++) {
        response.PCLoadData2CL.aQuestFlag[i] = plr.aQuestFlag[i];
    }

    // shut Computress up
    response.PCLoadData2CL.iFirstUseFlag1 = UINT64_MAX;
    response.PCLoadData2CL.iFirstUseFlag2 = UINT64_MAX;

    plr.SerialKey = enter->iEnterSerialKey;
    plr.instanceID = INSTANCE_OVERWORLD; // the player should never be in an instance on enter

    sock->setEKey(CNSocketEncryption::createNewKey(response.uiSvrTime, response.iID + 1, response.PCLoadData2CL.iFusionMatter + 1));
    sock->setFEKey(plr.FEKey);
    sock->setActiveKey(SOCKETKEY_FE); // send all packets using the FE key from now on

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ENTER_SUCC, sizeof(sP_FE2CL_REP_PC_ENTER_SUCC));

    // transmit MOTD after entering the game, so the client hopefully changes modes on time
    ChatManager::sendServerMessage(sock, settings::MOTDSTRING);

    addPlayer(sock, plr);
    // check if there is an expiring vehicle
    ItemManager::checkItemExpire(sock, getPlayer(sock));

    // set player equip stats
    ItemManager::setItemStats(getPlayer(sock));

    MissionManager::failInstancedMissions(sock);
}

void PlayerManager::sendToViewable(CNSocket* sock, void* buf, uint32_t type, size_t size) {
    for (Chunk* chunk : players[sock].currentChunks) {
        for (CNSocket* otherSock : chunk->players) {
            if (otherSock == sock)
                continue;

            otherSock->sendPacket(buf, type, size);
        }
    }
}

void PlayerManager::loadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LOADING_COMPLETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_LOADING_COMPLETE* complete = (sP_CL2FE_REQ_PC_LOADING_COMPLETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, response);
    Player *plr = getPlayer(sock);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_LOADING_COMPLETE:" << std::endl;
        std::cout << "\tPC_ID: " << complete->iPC_ID << std::endl;
    )

    response.iPC_ID = complete->iPC_ID;

    // reload players & NPCs
    updatePlayerPosition(sock, plr->x, plr->y, plr->z, plr->angle);

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC));
}

void PlayerManager::movePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVE* moveData = (sP_CL2FE_REQ_PC_MOVE*)data->buf;
    updatePlayerPosition(sock, moveData->iX, moveData->iY, moveData->iZ, moveData->iAngle);

    players[sock].plr->angle = moveData->iAngle;
    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVE, moveResponse);

    moveResponse.iID = players[sock].plr->iID;
    moveResponse.cKeyValue = moveData->cKeyValue;

    moveResponse.iX = moveData->iX;
    moveResponse.iY = moveData->iY;
    moveResponse.iZ = moveData->iZ;
    moveResponse.iAngle = moveData->iAngle;
    moveResponse.fVX = moveData->fVX;
    moveResponse.fVY = moveData->fVY;
    moveResponse.fVZ = moveData->fVZ;

    moveResponse.iSpeed = moveData->iSpeed;
    moveResponse.iCliTime = moveData->iCliTime; // maybe don't send this??? seems unneeded...
    moveResponse.iSvrTime = tm;

    sendToViewable(sock, (void*)&moveResponse, P_FE2CL_PC_MOVE, sizeof(sP_FE2CL_PC_MOVE));
}

void PlayerManager::stopPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_STOP))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_STOP* stopData = (sP_CL2FE_REQ_PC_STOP*)data->buf;
    updatePlayerPosition(sock, stopData->iX, stopData->iY, stopData->iZ);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_STOP, stopResponse);

    stopResponse.iID = players[sock].plr->iID;

    stopResponse.iX = stopData->iX;
    stopResponse.iY = stopData->iY;
    stopResponse.iZ = stopData->iZ;

    stopResponse.iCliTime = stopData->iCliTime; // maybe don't send this??? seems unneeded...
    stopResponse.iSvrTime = tm;

    sendToViewable(sock, (void*)&stopResponse, P_FE2CL_PC_STOP, sizeof(sP_FE2CL_PC_STOP));
}

void PlayerManager::jumpPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMP))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_JUMP* jumpData = (sP_CL2FE_REQ_PC_JUMP*)data->buf;
    updatePlayerPosition(sock, jumpData->iX, jumpData->iY, jumpData->iZ, jumpData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_JUMP, jumpResponse);

    jumpResponse.iID = players[sock].plr->iID;
    jumpResponse.cKeyValue = jumpData->cKeyValue;

    jumpResponse.iX = jumpData->iX;
    jumpResponse.iY = jumpData->iY;
    jumpResponse.iZ = jumpData->iZ;
    jumpResponse.iAngle = jumpData->iAngle;
    jumpResponse.iVX = jumpData->iVX;
    jumpResponse.iVY = jumpData->iVY;
    jumpResponse.iVZ = jumpData->iVZ;

    jumpResponse.iSpeed = jumpData->iSpeed;
    jumpResponse.iCliTime = jumpData->iCliTime; // maybe don't send this??? seems unneeded...
    jumpResponse.iSvrTime = tm;

    sendToViewable(sock, (void*)&jumpResponse, P_FE2CL_PC_JUMP, sizeof(sP_FE2CL_PC_JUMP));
}

void PlayerManager::jumppadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMPPAD))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_JUMPPAD* jumppadData = (sP_CL2FE_REQ_PC_JUMPPAD*)data->buf;
    updatePlayerPosition(sock, jumppadData->iX, jumppadData->iY, jumppadData->iZ, jumppadData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_JUMPPAD, jumppadResponse);

    jumppadResponse.iPC_ID = players[sock].plr->iID;
    jumppadResponse.cKeyValue = jumppadData->cKeyValue;

    jumppadResponse.iX = jumppadData->iX;
    jumppadResponse.iY = jumppadData->iY;
    jumppadResponse.iZ = jumppadData->iZ;
    jumppadResponse.iVX = jumppadData->iVX;
    jumppadResponse.iVY = jumppadData->iVY;
    jumppadResponse.iVZ = jumppadData->iVZ;

    jumppadResponse.iCliTime = jumppadData->iCliTime;
    jumppadResponse.iSvrTime = tm;

    sendToViewable(sock, (void*)&jumppadResponse, P_FE2CL_PC_JUMPPAD, sizeof(sP_FE2CL_PC_JUMPPAD));
}

void PlayerManager::launchPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LAUNCHER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_LAUNCHER* launchData = (sP_CL2FE_REQ_PC_LAUNCHER*)data->buf;
    updatePlayerPosition(sock, launchData->iX, launchData->iY, launchData->iZ, launchData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_LAUNCHER, launchResponse);

    launchResponse.iPC_ID = players[sock].plr->iID;

    launchResponse.iX = launchData->iX;
    launchResponse.iY = launchData->iY;
    launchResponse.iZ = launchData->iZ;
    launchResponse.iVX = launchData->iVX;
    launchResponse.iVY = launchData->iVY;
    launchResponse.iVZ = launchData->iVZ;
    launchResponse.iSpeed = launchData->iSpeed;
    launchResponse.iAngle = launchData->iAngle;

    launchResponse.iCliTime = launchData->iCliTime;
    launchResponse.iSvrTime = tm;

    sendToViewable(sock, (void*)&launchResponse, P_FE2CL_PC_LAUNCHER, sizeof(sP_FE2CL_PC_LAUNCHER));
}

void PlayerManager::ziplinePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ZIPLINE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ZIPLINE* ziplineData = (sP_CL2FE_REQ_PC_ZIPLINE*)data->buf;
    updatePlayerPosition(sock, ziplineData->iX, ziplineData->iY, ziplineData->iZ, ziplineData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_ZIPLINE, ziplineResponse);

    ziplineResponse.iPC_ID = players[sock].plr->iID;
    ziplineResponse.iCliTime = ziplineData->iCliTime;
    ziplineResponse.iSvrTime = tm;
    ziplineResponse.iX = ziplineData->iX;
    ziplineResponse.iY = ziplineData->iY;
    ziplineResponse.iZ = ziplineData->iZ;
    ziplineResponse.fVX = ziplineData->fVX;
    ziplineResponse.fVY = ziplineData->fVY;
    ziplineResponse.fVZ = ziplineData->fVZ;
    ziplineResponse.fMovDistance = ziplineData->fMovDistance;
    ziplineResponse.fMaxDistance = ziplineData->fMaxDistance;
    ziplineResponse.fDummy = ziplineData->fDummy; // wtf is this for?
    ziplineResponse.iStX = ziplineData->iStX;
    ziplineResponse.iStY = ziplineData->iStY;
    ziplineResponse.iStZ = ziplineData->iStZ;
    ziplineResponse.bDown = ziplineData->bDown;
    ziplineResponse.iSpeed = ziplineData->iSpeed;
    ziplineResponse.iAngle = ziplineData->iAngle;
    ziplineResponse.iRollMax = ziplineData->iRollMax;
    ziplineResponse.iRoll = ziplineData->iRoll;

    sendToViewable(sock, (void*)&ziplineResponse, P_FE2CL_PC_ZIPLINE, sizeof(sP_FE2CL_PC_ZIPLINE));
}

void PlayerManager::movePlatformPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVEPLATFORM* platformData = (sP_CL2FE_REQ_PC_MOVEPLATFORM*)data->buf;
    updatePlayerPosition(sock, platformData->iX, platformData->iY, platformData->iZ, platformData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVEPLATFORM, platResponse);

    platResponse.iPC_ID = players[sock].plr->iID;
    platResponse.iCliTime = platformData->iCliTime;
    platResponse.iSvrTime = tm;
    platResponse.iX = platformData->iX;
    platResponse.iY = platformData->iY;
    platResponse.iZ = platformData->iZ;
    platResponse.iAngle = platformData->iAngle;
    platResponse.fVX = platformData->fVX;
    platResponse.fVY = platformData->fVY;
    platResponse.fVZ = platformData->fVZ;
    platResponse.iLcX = platformData->iLcX;
    platResponse.iLcY = platformData->iLcY;
    platResponse.iLcZ = platformData->iLcZ;
    platResponse.iSpeed = platformData->iSpeed;
    platResponse.bDown = platformData->bDown;
    platResponse.cKeyValue = platformData->cKeyValue;
    platResponse.iPlatformID = platformData->iPlatformID;

    sendToViewable(sock, (void*)&platResponse, P_FE2CL_PC_MOVEPLATFORM, sizeof(sP_FE2CL_PC_MOVEPLATFORM));
}

void PlayerManager::moveSliderPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVETRANSPORTATION))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVETRANSPORTATION* sliderData = (sP_CL2FE_REQ_PC_MOVETRANSPORTATION*)data->buf;
    updatePlayerPosition(sock, sliderData->iX, sliderData->iY, sliderData->iZ, sliderData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_MOVETRANSPORTATION, sliderResponse);

    sliderResponse.iPC_ID = players[sock].plr->iID;
    sliderResponse.iCliTime = sliderData->iCliTime;
    sliderResponse.iSvrTime = tm;
    sliderResponse.iX = sliderData->iX;
    sliderResponse.iY = sliderData->iY;
    sliderResponse.iZ = sliderData->iZ;
    sliderResponse.iAngle = sliderData->iAngle;
    sliderResponse.fVX = sliderData->fVX;
    sliderResponse.fVY = sliderData->fVY;
    sliderResponse.fVZ = sliderData->fVZ;
    sliderResponse.iLcX = sliderData->iLcX;
    sliderResponse.iLcY = sliderData->iLcY;
    sliderResponse.iLcZ = sliderData->iLcZ;
    sliderResponse.iSpeed = sliderData->iSpeed;
    sliderResponse.cKeyValue = sliderData->cKeyValue;
    sliderResponse.iT_ID = sliderData->iT_ID;

    sendToViewable(sock, (void*)&sliderResponse, P_FE2CL_PC_MOVETRANSPORTATION, sizeof(sP_FE2CL_PC_MOVETRANSPORTATION));
}

void PlayerManager::moveSlopePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_SLOPE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_SLOPE* slopeData = (sP_CL2FE_REQ_PC_SLOPE*)data->buf;
    updatePlayerPosition(sock, slopeData->iX, slopeData->iY, slopeData->iZ,slopeData->iAngle);

    uint64_t tm = getTime();

    INITSTRUCT(sP_FE2CL_PC_SLOPE, slopeResponse);

    slopeResponse.iPC_ID = players[sock].plr->iID;
    slopeResponse.iCliTime = slopeData->iCliTime;
    slopeResponse.iSvrTime = tm;
    slopeResponse.iX = slopeData->iX;
    slopeResponse.iY = slopeData->iY;
    slopeResponse.iZ = slopeData->iZ;
    slopeResponse.iAngle = slopeData->iAngle;
    slopeResponse.fVX = slopeData->fVX;
    slopeResponse.fVY = slopeData->fVY;
    slopeResponse.fVZ = slopeData->fVZ;
    slopeResponse.iSpeed = slopeData->iSpeed;
    slopeResponse.cKeyValue = slopeData->cKeyValue;
    slopeResponse.iSlopeID = slopeData->iSlopeID;

    sendToViewable(sock, (void*)&slopeResponse, P_FE2CL_PC_SLOPE, sizeof(sP_FE2CL_PC_SLOPE));
}

void PlayerManager::gotoPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GOTO))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GOTO* gotoData = (sP_CL2FE_REQ_PC_GOTO*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, response);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_GOTO:" << std::endl;
        std::cout << "\tX: " << gotoData->iToX << std::endl;
        std::cout << "\tY: " << gotoData->iToY << std::endl;
        std::cout << "\tZ: " << gotoData->iToZ << std::endl;
        )

    sendPlayerTo(sock, gotoData->iToX, gotoData->iToY, gotoData->iToZ, 0);
}

void PlayerManager::setSpecialPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE))
        return; // ignore the malformed packet

    sP_CL2FE_GM_REQ_PC_SET_VALUE* setData = (sP_CL2FE_GM_REQ_PC_SET_VALUE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, response);

    DEBUGLOG(
        std::cout << "P_CL2FE_GM_REQ_PC_SET_VALUE:" << std::endl;
        std::cout << "\tPC_ID: " << setData->iPC_ID << std::endl;
        std::cout << "\tSetValueType: " << setData->iSetValueType << std::endl;
        std::cout << "\tSetValue: " << setData->iSetValue << std::endl;
    )

    // Handle serverside value-changes
    switch (setData->iSetValueType) {
    case 1:
        plr->HP = setData->iSetValue;
        break;
    case 2:
        plr->batteryW = setData->iSetValue;
        break;
    case 3:
        plr->batteryN = setData->iSetValue;
        break;
    case 4:
        plr->fusionmatter = setData->iSetValue;
        break;
    case 5:
        plr->money = setData->iSetValue;
        break;
    default:
        break;
    }

    response.iPC_ID = setData->iPC_ID;
    response.iSetValue = setData->iSetValue;
    response.iSetValueType = setData->iSetValueType;

    sock->sendPacket((void*)&response, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));
}

void PlayerManager::heartbeatPlayer(CNSocket* sock, CNPacketData* data) {
    players[sock].lastHeartbeat = getTime();
}

void PlayerManager::exitGame(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_EXIT))
        return;

    sP_CL2FE_REQ_PC_EXIT* exitData = (sP_CL2FE_REQ_PC_EXIT*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_EXIT_SUCC, response);

    response.iID = exitData->iID;
    response.iExitCode = 1;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_EXIT_SUCC, sizeof(sP_FE2CL_REP_PC_EXIT_SUCC));
}

void PlayerManager::revivePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_REGEN))
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    if (plr == nullptr)
        return;

    WarpLocation target = PlayerManager::getRespawnPoint(plr);

    sP_CL2FE_REQ_PC_REGEN* reviveData = (sP_CL2FE_REQ_PC_REGEN*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_REGEN_SUCC, response);
    INITSTRUCT(sP_FE2CL_PC_REGEN, resp2);

    int activeSlot = -1;

    if (reviveData->iRegenType == 3 && plr->iConditionBitFlag & CSB_BIT_PHOENIX) {
        // nano revive
        plr->Nanos[plr->activeNano].iStamina = 0;
        NanoManager::nanoUnbuff(sock, CSB_BIT_PHOENIX, ECSB_PHOENIX, 0, false);
        plr->HP = PC_MAXHEALTH(plr->level);
    } else {
        updatePlayerPosition(sock, target.x, target.y, target.z);

        if (reviveData->iRegenType != 5)
            plr->HP = PC_MAXHEALTH(plr->level);

        for (int i = 0; i < 3; i++) {
            int nanoID = plr->equippedNanos[i];

            // halve nano health if respawning
            if (reviveData->iRegenType != 5)
                plr->Nanos[nanoID].iStamina = 75; // max is 150, so 75 is half
            response.PCRegenData.Nanos[i] = plr->Nanos[nanoID];
            if (plr->activeNano == nanoID)
                activeSlot = i;
        }
    }

    // Response parameters
    response.PCRegenData.iActiveNanoSlotNum = activeSlot;
    response.PCRegenData.iX = plr->x;
    response.PCRegenData.iY = plr->y;
    response.PCRegenData.iZ = plr->z;
    response.PCRegenData.iHP = plr->HP;
    response.iFusionMatter = plr->fusionmatter;
    response.bMoveLocation = 0;
    response.PCRegenData.iMapNum = 0;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_REGEN_SUCC, sizeof(sP_FE2CL_REP_PC_REGEN_SUCC));

    // Update other players
    resp2.PCRegenDataForOtherPC.iPC_ID = plr->iID;
    resp2.PCRegenDataForOtherPC.iX = plr->x;
    resp2.PCRegenDataForOtherPC.iY = plr->y;
    resp2.PCRegenDataForOtherPC.iZ = plr->z;
    resp2.PCRegenDataForOtherPC.iHP = plr->HP;
    resp2.PCRegenDataForOtherPC.iAngle = plr->angle;
    resp2.PCRegenDataForOtherPC.iConditionBitFlag = plr->iConditionBitFlag;
    resp2.PCRegenDataForOtherPC.iPCState = plr->iPCState;
    resp2.PCRegenDataForOtherPC.iSpecialState = plr->iSpecialState;
    resp2.PCRegenDataForOtherPC.Nano = plr->Nanos[plr->activeNano];

    sendToViewable(sock, (void*)&resp2, P_FE2CL_PC_REGEN, sizeof(sP_FE2CL_PC_REGEN));
}

void PlayerManager::enterPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    PlayerView& plr = PlayerManager::players[sock];

    if (plr.plr->Equip[8].iID > 0 && plr.plr->Equip[8].iTimeLimit>getTimestamp()) {
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_ON_SUCC, response);
        sock->sendPacket((void*)&response, P_FE2CL_PC_VEHICLE_ON_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_ON_SUCC));

        // send to other players
        plr.plr->iPCState = 8;
        INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, response2);
        response2.iPC_ID = plr.plr->iID;
        response2.iState = 8;

        for (Chunk* chunk : players[sock].currentChunks) {
            for (CNSocket* otherSock : chunk->players) {
                otherSock->sendPacket((void*)&response2, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
            }
        }
    } else {
        INITSTRUCT(sP_FE2CL_PC_VEHICLE_ON_FAIL, response);
        sock->sendPacket((void*)&response, P_FE2CL_PC_VEHICLE_ON_FAIL, sizeof(sP_FE2CL_PC_VEHICLE_ON_FAIL));

        // check if vehicle didn't expire
        if (plr.plr->Equip[8].iTimeLimit < getTimestamp()) {
            plr.plr->toRemoveVehicle.eIL = 0;
            plr.plr->toRemoveVehicle.iSlotNum = 8;
            ItemManager::checkItemExpire(sock, plr.plr);
        }
    }
}

void PlayerManager::exitPlayerVehicle(CNSocket* sock, CNPacketData* data) {

    INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, response);
    sock->sendPacket((void*)&response, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));

    PlayerView plr = PlayerManager::players[sock];

    // send to other players
    plr.plr->iPCState = 0;
    INITSTRUCT(sP_FE2CL_PC_STATE_CHANGE, response2);
    response2.iPC_ID = plr.plr->iID;
    response2.iState = 0;

    sendToViewable(sock, (void*)&response2, P_FE2CL_PC_STATE_CHANGE, sizeof(sP_FE2CL_PC_STATE_CHANGE));
}

void PlayerManager::setSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data) {
    setSpecialState(sock, data);
}

void PlayerManager::setGMSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data) {

    if (getPlayer(sock)->accountLevel > 30)
        return;

    setSpecialState(sock, data);
}

void PlayerManager::changePlayerGuide(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_CHANGE_MENTOR))
        return;

    sP_CL2FE_REQ_PC_CHANGE_MENTOR *pkt = (sP_CL2FE_REQ_PC_CHANGE_MENTOR*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC, resp);
    Player *plr = getPlayer(sock);

    resp.iMentor = pkt->iMentor;
    resp.iMentorCnt = 1;
    resp.iFusionMatter = plr->fusionmatter; // no cost

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_CHANGE_MENTOR_SUCC, sizeof(sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC));
    // if it's changed from computress
    if (plr->mentor == 5) {
        // we're warping to the past
        plr->PCStyle2.iPayzoneFlag = 1;
        // remove all active missions
        for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
            if (plr->tasks[i] != 0)
                MissionManager::quitTask(sock, plr->tasks[i], true);
        }

        // start Blossom nano mission if applicable
        MissionManager::updateFusionMatter(sock, 0);
    }
    // save it on player
    plr->mentor = pkt->iMentor;
}

#pragma region Helper methods
Player *PlayerManager::getPlayer(CNSocket* key) {
    if (players.find(key) != players.end())
        return players[key].plr;

    return nullptr;
}

WarpLocation PlayerManager::getRespawnPoint(Player *plr) {
    WarpLocation best;
    uint32_t curDist, bestDist = UINT32_MAX;

    for (auto targ : NPCManager::RespawnPoints) {
        curDist = sqrt(pow(plr->x - targ.x, 2) + pow(plr->y - targ.y, 2));
        if (curDist < bestDist && targ.instanceID == MAPNUM(plr->instanceID)) { // only mapNum needs to match
            best = targ;
            bestDist = curDist;
        }
    }

    return best;
}

bool PlayerManager::isAccountInUse(int accountId) {
    std::map<CNSocket*, PlayerView>::iterator it;
    for (it = PlayerManager::players.begin(); it != PlayerManager::players.end(); it++) {
        if (it->second.plr->accountId == accountId)
            return true;
    }
    return false;
}

void PlayerManager::exitDuplicate(int accountId) {
    std::map<CNSocket*, PlayerView>::iterator it;

    // disconnect any duplicate players
    for (it = players.begin(); it != players.end(); it++) {
        if (it->second.plr->accountId == accountId) {
            CNSocket* sock = it->first;

            INITSTRUCT(sP_FE2CL_REP_PC_EXIT_DUPLICATE, resp);
            resp.iErrorCode = 0;
            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_EXIT_DUPLICATE, sizeof(sP_FE2CL_REP_PC_EXIT_DUPLICATE));

            sock->kill();
            CNShardServer::_killConnection(sock);
        }
    }
}

void PlayerManager::setSpecialState(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH))
        return; // ignore the malformed packet

    Player *plr = getPlayer(sock);

    if (plr == nullptr)
        return;

    sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH* setData = (sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH*)data->buf;

    // HACK: work around the invisible weapon bug
    if (setData->iSpecialStateFlag == CN_SPECIAL_STATE_FLAG__FULL_UI)
        ItemManager::updateEquips(sock, plr);

    INITSTRUCT(sP_FE2CL_PC_SPECIAL_STATE_CHANGE, response);

    plr->iSpecialState ^= setData->iSpecialStateFlag;

    response.iPC_ID = setData->iPC_ID;
    response.iReqSpecialStateFlag = setData->iSpecialStateFlag;
    response.iSpecialState = plr->iSpecialState;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC));
    sendToViewable(sock, (void*)&response, P_FE2CL_PC_SPECIAL_STATE_CHANGE, sizeof(sP_FE2CL_PC_SPECIAL_STATE_CHANGE));
}

Player *PlayerManager::getPlayerFromID(int32_t iID) {
    for (auto& pair : PlayerManager::players)
        if (pair.second.plr->iID == iID)
            return pair.second.plr;

    return nullptr;
}

CNSocket *PlayerManager::getSockFromID(int32_t iID) {
    for (auto& pair : PlayerManager::players)
        if (pair.second.plr->iID == iID)
            return pair.first;

    return nullptr;
}
#pragma endregion
