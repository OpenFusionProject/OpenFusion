#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "CNShardServer.hpp"
#include "CNShared.hpp"

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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SLOPE, PlayerManager::moveSlopePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GOTO, PlayerManager::gotoPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SET_VALUE, PlayerManager::setSpecialPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REP_LIVE_CHECK, PlayerManager::heartbeatPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_REGEN, PlayerManager::revivePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EXIT, PlayerManager::exitGame);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH, PlayerManager::setSpecialSwitchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_ON, PlayerManager::enterPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_VEHICLE_OFF, PlayerManager::exitPlayerVehicle);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_CHANGE_MENTOR, PlayerManager::changePlayerGuide);
}

void PlayerManager::addPlayer(CNSocket* key, Player plr) {
    Player *p = new Player();

    memcpy(p, &plr, sizeof(Player));

    players[key] = PlayerView();
    players[key].chunkPos = std::make_pair<int, int>(0, 0);
    players[key].currentChunks = std::vector<Chunk*>();
    players[key].plr = p;
    players[key].lastHeartbeat = 0;

    key->plr = p;

    std::cout << U16toU8(plr.PCStyle.szFirstName) << " " << U16toU8(plr.PCStyle.szLastName) << " has joined!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayer(CNSocket* key) {
    PlayerView& view = players[key];

    INITSTRUCT(sP_FE2CL_PC_EXIT, exitPacket);
    exitPacket.iID = players[key].plr->iID;

    // remove players from all chunks
    removePlayerFromChunks(view.currentChunks, key);

    // remove from chunk
    if (ChunkManager::chunks.find(view.chunkPos) != ChunkManager::chunks.end())
        ChunkManager::chunks[view.chunkPos]->players.erase(key);

    std::cout << U16toU8(view.plr->PCStyle.szFirstName) << " " << U16toU8(view.plr->PCStyle.szLastName) << " (PlayerId = " << view.plr->iID << ") has left!" << std::endl;

    key->plr = nullptr;
    delete view.plr;
    players.erase(key);

    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayerFromChunks(std::vector<Chunk*> chunks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
    INITSTRUCT(sP_FE2CL_PC_EXIT, exitPlayer);

    // for chunks that need the player to be removed from
    for (Chunk* chunk : chunks) {

        // remove NPCs
        for (int32_t id : chunk->NPCs) {
            exitData.iNPC_ID = id;
            sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
        }

        // remove players from eachother
        for (CNSocket* otherSock : chunk->players) {
            exitPlayer.iID = players[sock].plr->iID;
            otherSock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
            exitPlayer.iID = players[otherSock].plr->iID;
            sock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
        }
    }

    // remove us from that old stinky chunk (+ a sanity check)
    if (ChunkManager::chunks.find(players[sock].chunkPos) != ChunkManager::chunks.end())
        ChunkManager::chunks[players[sock].chunkPos]->players.erase(sock);
}

void PlayerManager::addPlayerToChunks(std::vector<Chunk*> chunks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
    INITSTRUCT(sP_FE2CL_PC_NEW, newPlayer);

    for (Chunk* chunk : chunks) {
        // add npcs
        for (int32_t id : chunk->NPCs) {
            enterData.NPCAppearanceData = NPCManager::NPCs[id]->appearanceData;
            sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
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

    std::pair<int, int> newPos = ChunkManager::grabChunk(X, Y);

    // nothing to be done
    if (newPos == view.chunkPos) 
        return;

    // add player to chunk
    std::vector<Chunk*> allChunks = ChunkManager::grabChunks(newPos.first, newPos.second);

    // first, remove all the old npcs & players from the old chunks
    removePlayerFromChunks(ChunkManager::getDeltaChunks(view.currentChunks, allChunks), sock);
    
    // now, add all the new npcs & players!
    addPlayerToChunks(ChunkManager::getDeltaChunks(allChunks, view.currentChunks), sock);
    
    ChunkManager::addPlayer(X, Y, sock); // takes care of adding the player to the chunk if it exists or not
    view.chunkPos = newPos;
    view.currentChunks = allChunks;
}

void PlayerManager::enterPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ENTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ENTER* enter = (sP_CL2FE_REQ_PC_ENTER*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_ENTER_SUCC, response);
    INITSTRUCT(sP_FE2CL_PC_MOTD_LOGIN, motd);

    // TODO: check if serialkey exists, if it doesn't send sP_FE2CL_REP_PC_ENTER_FAIL
    Player plr = CNSharedData::getPlayer(enter->iEnterSerialKey);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_ENTER:" << std::endl;
        std::cout << "\tID: " << U16toU8(enter->szID) << std::endl;
        std::cout << "\tSerial: " << enter->iEnterSerialKey << std::endl;
        std::cout << "\tTemp: " << enter->iTempValue << std::endl;
        std::cout << "\tPC_UID: " << plr.PCStyle.iPC_UID << std::endl;
    )

    response.iID = plr.iID;
    response.uiSvrTime = getTime();
    response.PCLoadData2CL.iUserLevel = plr.level;
    response.PCLoadData2CL.iHP = plr.HP;
    response.PCLoadData2CL.iLevel = plr.level;
    response.PCLoadData2CL.iCandy = plr.money;
    response.PCLoadData2CL.iMentor = 5; // Computress
    response.PCLoadData2CL.iMentorCount = 1; // how many guides the player has had
    response.PCLoadData2CL.iMapNum = 0;
    response.PCLoadData2CL.iX = plr.x;
    response.PCLoadData2CL.iY = plr.y;
    response.PCLoadData2CL.iZ = plr.z;
    response.PCLoadData2CL.iAngle = plr.angle;

    response.PCLoadData2CL.iActiveNanoSlotNum = -1;
    response.PCLoadData2CL.iFatigue = 50;
    response.PCLoadData2CL.PCStyle = plr.PCStyle;
    response.PCLoadData2CL.PCStyle2 = plr.PCStyle2;
    // inventory
    for (int i = 0; i < AEQUIP_COUNT; i++)
        response.PCLoadData2CL.aEquip[i] = plr.Equip[i];

    for (int i = 0; i < AINVEN_COUNT; i++)
        response.PCLoadData2CL.aInven[i] = plr.Inven[i];
    // nanos
    for (int i = 1; i < SIZEOF_NANO_BANK_SLOT; i++) {
        response.PCLoadData2CL.aNanoBank[i] = plr.Nanos[i];
    }
    for (int i = 0; i < 3; i++) {
        response.PCLoadData2CL.aNanoSlots[i] = plr.equippedNanos[i];
    }

    // missions
    for (int i = 0; i < 16; i++) {
        response.PCLoadData2CL.aQuestFlag[i] = plr.aQuestFlag[i];
    }

    // shut Computress up
    response.PCLoadData2CL.iFirstUseFlag1 = UINT64_MAX;
    response.PCLoadData2CL.iFirstUseFlag2 = UINT64_MAX;

    plr.SerialKey = enter->iEnterSerialKey;

    motd.iType = 1;
    U8toU16(settings::MOTDSTRING, (char16_t*)motd.szSystemMsg);

    sock->setEKey(CNSocketEncryption::createNewKey(response.uiSvrTime, response.iID + 1, response.PCLoadData2CL.iFusionMatter + 1));
    sock->setFEKey(plr.FEKey);
    sock->setActiveKey(SOCKETKEY_FE); // send all packets using the FE key from now on

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_ENTER_SUCC, sizeof(sP_FE2CL_REP_PC_ENTER_SUCC));

    // transmit MOTD after entering the game, so the client hopefully changes modes on time
    sock->sendPacket((void*)&motd, P_FE2CL_PC_MOTD_LOGIN, sizeof(sP_FE2CL_PC_MOTD_LOGIN));

    addPlayer(sock, plr);
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
    PlayerView& plrv = players[sock];

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_GOTO:" << std::endl;
        std::cout << "\tX: " << gotoData->iToX << std::endl;
        std::cout << "\tY: " << gotoData->iToY << std::endl;
        std::cout << "\tZ: " << gotoData->iToZ << std::endl;
    )

    response.iX = plrv.plr->x = gotoData->iToX;
    response.iY = plrv.plr->y = gotoData->iToY;
    response.iZ = plrv.plr->z = gotoData->iToZ;

    // force player & NPC reload
    PlayerManager::removePlayerFromChunks(plrv.currentChunks, sock);
    plrv.currentChunks.clear();
    plrv.chunkPos = std::make_pair<int, int>(0, 0);

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));
}

void PlayerManager::setSpecialPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE))
        return; // ignore the malformed packet

    sP_CL2FE_GM_REQ_PC_SET_VALUE* setData = (sP_CL2FE_GM_REQ_PC_SET_VALUE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

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
        // TODO: batteryW
        break;
    case 3:
        // TODO: batteryN nanopotion
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
    WarpLocation target = PlayerManager::getRespawnPoint(plr);

    sP_CL2FE_REQ_PC_REGEN* reviveData = (sP_CL2FE_REQ_PC_REGEN*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_REGEN_SUCC, response);
    INITSTRUCT(sP_FE2CL_PC_REGEN, resp2);

    // Nanos
    for (int n = 0; n < 3; n++) {
        int nanoID = plr->equippedNanos[n];
        plr->Nanos[nanoID].iStamina = 75; // max is 150, so 75 is half
        response.PCRegenData.Nanos[n] = plr->Nanos[nanoID];
    }

    // Update player
    plr->x = target.x;
    plr->y = target.y;
    plr->z = target.z;
    plr->HP = 1000 * plr->level;

    // Response parameters
    response.PCRegenData.iActiveNanoSlotNum = plr->activeNano;
    response.PCRegenData.iX = plr->x;
    response.PCRegenData.iY = plr->y;
    response.PCRegenData.iZ = plr->z;
    response.PCRegenData.iHP = plr->HP;
    response.iFusionMatter = plr->fusionmatter;
    response.bMoveLocation = reviveData->eIL;
    response.PCRegenData.iMapNum = reviveData->iIndex;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_REGEN_SUCC, sizeof(sP_FE2CL_REP_PC_REGEN_SUCC));

    // Update other players
    resp2.PCRegenDataForOtherPC.iPC_ID = plr->iID;
    resp2.PCRegenDataForOtherPC.iX = plr->x;
    resp2.PCRegenDataForOtherPC.iY = plr->y;
    resp2.PCRegenDataForOtherPC.iZ = plr->z;
    resp2.PCRegenDataForOtherPC.iHP = plr->HP;
    resp2.PCRegenDataForOtherPC.iAngle = plr->angle;
    resp2.PCRegenDataForOtherPC.Nano = plr->Nanos[plr->activeNano];

    sendToViewable(sock, (void*)&resp2, P_FE2CL_PC_REGEN, sizeof(sP_FE2CL_PC_REGEN));
}

void PlayerManager::enterPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    PlayerView& plr = PlayerManager::players[sock];

    if (plr.plr->Equip[8].iID > 0) {
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
    sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH* specialData = (sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, response);

    response.iPC_ID = specialData->iPC_ID;
    response.iReqSpecialStateFlag = specialData->iSpecialStateFlag;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC));
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
}

#pragma region Helper methods
Player *PlayerManager::getPlayer(CNSocket* key) {
    assert(key->plr != nullptr);
    return key->plr;
}

WarpLocation PlayerManager::getRespawnPoint(Player *plr) {
    WarpLocation best;
    uint32_t curDist, bestDist = UINT32_MAX;

    for (auto targ : NPCManager::RespawnPoints) {
        curDist = sqrt(pow(plr->x - targ.x, 2) + pow(plr->y - targ.y, 2));
        if (curDist < bestDist) {
            best = targ;
            bestDist = curDist;
        }
    }

    return best;
}
bool PlayerManager::isAccountInUse(int accountId) {
    std::map<CNSocket*, PlayerView>::iterator it;
    for (it = PlayerManager::players.begin(); it != PlayerManager::players.end(); it++)
    {
        if (it->second.plr->accountId == accountId)
            return true;
    }
    return false;
}

void PlayerManager::exitDuplicate(int accountId) {
    std::map<CNSocket*, PlayerView>::iterator it;
    for (it = PlayerManager::players.begin(); it != PlayerManager::players.end(); it++)
    {
        if (it->second.plr->accountId == accountId)
        {
            CNSocket* sock = it->first;
            INITSTRUCT(sP_FE2CL_REP_PC_EXIT_DUPLICATE, resp);
            resp.iErrorCode = 0;
            sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_EXIT_DUPLICATE, sizeof(sP_FE2CL_REP_PC_EXIT_DUPLICATE));
            sock->kill();
        }
    }
}
#pragma endregion
