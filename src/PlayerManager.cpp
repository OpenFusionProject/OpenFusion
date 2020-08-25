#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "CNShardServer.hpp"
#include "CNShared.hpp"

#include "settings.hpp"

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
}

void PlayerManager::addPlayer(CNSocket* key, Player plr) {
    Player *p = new Player();

    memcpy(p, &plr, sizeof(Player));

    players[key] = PlayerView();
    players[key].viewable = std::list<CNSocket*>();
    players[key].plr = p;
    players[key].lastHeartbeat = 0;

    std::cout << U16toU8(plr.PCStyle.szFirstName) << " " << U16toU8(plr.PCStyle.szLastName) << " has joined!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayer(CNSocket* key) {
    PlayerView cachedView = players[key];

    // if players have them in their viewable lists, remove it
    for (CNSocket* otherSock : players[key].viewable) {
        players[otherSock].viewable.remove(key); // gone

        // now sent PC_EXIT packet
        sP_FE2CL_PC_EXIT exitPacket;
        exitPacket.iID = players[key].plr->iID;

        otherSock->sendPacket((void*)&exitPacket, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
    }

    delete cachedView.plr;
    players.erase(key);

    std::cout << U16toU8(cachedView.plr->PCStyle.szFirstName) << U16toU8(cachedView.plr->PCStyle.szLastName) << " has left!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::updatePlayerPosition(CNSocket* sock, int X, int Y, int Z) {
    players[sock].plr->x = X;
    players[sock].plr->y = Y;
    players[sock].plr->z = Z;

    std::vector<CNSocket*> noView;
    std::vector<CNSocket*> yesView;

    // TODO: oh god this is sooooo perfomance heavy the more players you have
    for (auto pair : players) {
        if (pair.first == sock)
            continue; // ignore our own connection

        int diffX = abs(pair.second.plr->x - X); // the map is like a grid, X and Y are your position on the map, Z is the height. very different from other games...
        int diffY = abs(pair.second.plr->y - Y);

        if (diffX < settings::VIEWDISTANCE && diffY < settings::VIEWDISTANCE) {
            yesView.push_back(pair.first);
        }
        else {
            noView.push_back(pair.first);
        }
    }

    INITSTRUCT(sP_FE2CL_PC_EXIT, exitPacket);
    std::list<CNSocket*>::iterator i = players[sock].viewable.begin();
    while (i != players[sock].viewable.end()) {
        CNSocket* otherSock = *i;
        if (std::find(noView.begin(), noView.end(), otherSock) != noView.end()) {

            // sock shouldn't be visible, send PC_EXIT packet
            exitPacket.iID = players[sock].plr->iID;
            otherSock->sendPacket((void*)&exitPacket, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
            exitPacket.iID = players[otherSock].plr->iID;
            sock->sendPacket((void*)&exitPacket, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));

            // remove them from the viewable list
            players[sock].viewable.erase(i++);
            players[otherSock].viewable.remove(sock);
            continue;
        }

        ++i;
    }

    INITSTRUCT(sP_FE2CL_PC_NEW, newPlayer);
    for (CNSocket* otherSock : yesView) {
        if (std::find(players[sock].viewable.begin(), players[sock].viewable.end(), otherSock) == players[sock].viewable.end()) {
            // this needs to be added to the viewable players, send PC_ENTER

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

            players[sock].viewable.push_back(otherSock);
            players[otherSock].viewable.push_back(sock);
        }
    }

    NPCManager::updatePlayerNPCS(sock, players[sock]);
}

std::list<CNSocket*> PlayerManager::getNearbyPlayers(int x, int y, int dist) {
    std::list<CNSocket*> plrs;

    for (auto pair : players) {
        int diffX = abs(pair.second.plr->x - x);
        int diffY = abs(pair.second.plr->x - x);

        if (diffX < dist && diffY < dist)
            plrs.push_back(pair.first);
    }

    return plrs;
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

    response.iID = rand();
    response.uiSvrTime = getTime();
    response.PCLoadData2CL.iUserLevel = 1;
    response.PCLoadData2CL.iHP = 3625; //TODO: Check player levelupdata and get this right
    response.PCLoadData2CL.iLevel = plr.level;
    response.PCLoadData2CL.iMentor = 1;
    response.PCLoadData2CL.iMentorCount = 4;
    response.PCLoadData2CL.iMapNum = 0;
    response.PCLoadData2CL.iX = plr.x;
    response.PCLoadData2CL.iY = plr.y;
    response.PCLoadData2CL.iZ = plr.z;
    response.PCLoadData2CL.iActiveNanoSlotNum = -1;
    response.PCLoadData2CL.iFatigue = 50;
    response.PCLoadData2CL.PCStyle = plr.PCStyle;
    response.PCLoadData2CL.PCStyle2 = plr.PCStyle2;

    for (int i = 0; i < AEQUIP_COUNT; i++)
        response.PCLoadData2CL.aEquip[i] = plr.Equip[i];

    for (int i = 0; i < AINVEN_COUNT; i++)
        response.PCLoadData2CL.aInven[i] = plr.Inven[i];

    // don't ask..
    for (int i = 1; i < 37; i++) {
        response.PCLoadData2CL.aNanoBank[i].iID = i;
        response.PCLoadData2CL.aNanoBank[i].iSkillID = 1;
        response.PCLoadData2CL.aNanoBank[i].iStamina = 150;
    }

    // temporarily not add nanos for nano add test through commands
    //response.PCLoadData2CL.aNanoSlots[0] = 1;
    //response.PCLoadData2CL.aNanoSlots[1] = 2;
    //response.PCLoadData2CL.aNanoSlots[2] = 3;

    response.PCLoadData2CL.aQuestFlag[0] = -1;

    plr.iID = response.iID;
    plr.SerialKey = enter->iEnterSerialKey;
    plr.HP = response.PCLoadData2CL.iHP;

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

void PlayerManager::loadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LOADING_COMPLETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_LOADING_COMPLETE* complete = (sP_CL2FE_REQ_PC_LOADING_COMPLETE*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, response);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_LOADING_COMPLETE:" << std::endl;
        std::cout << "\tPC_ID: " << complete->iPC_ID << std::endl;
    )

    response.iPC_ID = complete->iPC_ID;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC));
}

void PlayerManager::movePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVE* moveData = (sP_CL2FE_REQ_PC_MOVE*)data->buf;
    updatePlayerPosition(sock, moveData->iX, moveData->iY, moveData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&moveResponse, P_FE2CL_PC_MOVE, sizeof(sP_FE2CL_PC_MOVE));
    }
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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&stopResponse, P_FE2CL_PC_STOP, sizeof(sP_FE2CL_PC_STOP));
    }
}

void PlayerManager::jumpPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMP))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_JUMP* jumpData = (sP_CL2FE_REQ_PC_JUMP*)data->buf;
    updatePlayerPosition(sock, jumpData->iX, jumpData->iY, jumpData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&jumpResponse, P_FE2CL_PC_JUMP, sizeof(sP_FE2CL_PC_JUMP));
    }
}

void PlayerManager::jumppadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMPPAD))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_JUMPPAD* jumppadData = (sP_CL2FE_REQ_PC_JUMPPAD*)data->buf;
    updatePlayerPosition(sock, jumppadData->iX, jumppadData->iY, jumppadData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&jumppadResponse, P_FE2CL_PC_JUMPPAD, sizeof(sP_FE2CL_PC_JUMPPAD));
    }
}

void PlayerManager::launchPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LAUNCHER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_LAUNCHER* launchData = (sP_CL2FE_REQ_PC_LAUNCHER*)data->buf;
    updatePlayerPosition(sock, launchData->iX, launchData->iY, launchData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&launchResponse, P_FE2CL_PC_LAUNCHER, sizeof(sP_FE2CL_PC_LAUNCHER));
    }
}

void PlayerManager::ziplinePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ZIPLINE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ZIPLINE* ziplineData = (sP_CL2FE_REQ_PC_ZIPLINE*)data->buf;
    updatePlayerPosition(sock, ziplineData->iX, ziplineData->iY, ziplineData->iZ);

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
    ziplineResponse.fDummy = ziplineData->fDummy; //wtf is this for?
    ziplineResponse.iStX = ziplineData->iStX;
    ziplineResponse.iStY = ziplineData->iStY;
    ziplineResponse.iStZ = ziplineData->iStZ;
    ziplineResponse.bDown = ziplineData->bDown;
    ziplineResponse.iSpeed = ziplineData->iSpeed;
    ziplineResponse.iAngle = ziplineData->iAngle;
    ziplineResponse.iRollMax = ziplineData->iRollMax;
    ziplineResponse.iRoll = ziplineData->iRoll;

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&ziplineResponse, P_FE2CL_PC_ZIPLINE, sizeof(sP_FE2CL_PC_ZIPLINE));
    }
}

void PlayerManager::movePlatformPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVEPLATFORM* platformData = (sP_CL2FE_REQ_PC_MOVEPLATFORM*)data->buf;
    updatePlayerPosition(sock, platformData->iX, platformData->iY, platformData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&platResponse, P_FE2CL_PC_MOVEPLATFORM, sizeof(sP_FE2CL_PC_MOVEPLATFORM));
    }
}

void PlayerManager::moveSlopePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_SLOPE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_SLOPE* slopeData = (sP_CL2FE_REQ_PC_SLOPE*)data->buf;
    updatePlayerPosition(sock, slopeData->iX, slopeData->iY, slopeData->iZ);

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

    for (CNSocket* otherSock : players[sock].viewable) {
        otherSock->sendPacket((void*)&slopeResponse, P_FE2CL_PC_SLOPE, sizeof(sP_FE2CL_PC_SLOPE));
    }
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

    response.iX = gotoData->iToX;
    response.iY = gotoData->iToY;
    response.iZ = gotoData->iToZ;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));
}

void PlayerManager::setSpecialPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE))
        return; // ignore the malformed packet

    sP_CL2FE_GM_REQ_PC_SET_VALUE* setData = (sP_CL2FE_GM_REQ_PC_SET_VALUE*)data->buf;
    INITSTRUCT(sP_FE2CL_GM_REP_PC_SET_VALUE, response);

    DEBUGLOG(
        std::cout << "P_CL2FE_GM_REQ_PC_SET_VALUE:" << std::endl;
        std::cout << "\tPC_ID: " << setData->iPC_ID << std::endl;
        std::cout << "\tSetValueType: " << setData->iSetValueType << std::endl;
        std::cout << "\tSetValue: " << setData->iSetValue << std::endl;
    )

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

    // players respawn at same spot they died at for now...
    sP_CL2FE_REQ_PC_REGEN* reviveData = (sP_CL2FE_REQ_PC_REGEN*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_REGEN_SUCC, response);
    response.bMoveLocation = reviveData->eIL;
    response.PCRegenData.iMapNum = reviveData->iIndex;
    response.PCRegenData.iHP = 1000 * plr->level;
    response.PCRegenData.iX = plr->x;
    response.PCRegenData.iY = plr->y;
    response.PCRegenData.iZ = plr->z;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_REGEN_SUCC, sizeof(sP_FE2CL_REP_PC_REGEN_SUCC));
}

void PlayerManager::enterPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_PC_VEHICLE_ON_SUCC, response);
    PlayerView plrv = PlayerManager::players[sock];

    //send to other players
    INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, pkt);
    pkt.EquipSlotItem.iType = 1;
    pkt.iEquipSlotNum = 8;
    for (CNSocket* otherSock : plrv.viewable) {
        otherSock->sendPacket((void*)&pkt, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
    }

    plrv.plr->iPCState = 8;

    sock->sendPacket((void*)&response, P_FE2CL_PC_VEHICLE_ON_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_ON_SUCC));
}

void PlayerManager::exitPlayerVehicle(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_PC_VEHICLE_OFF_SUCC, response);
    PlayerView plrv = PlayerManager::players[sock];

    //send to other players
    INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, pkt);
    pkt.EquipSlotItem.iType = 1;
    pkt.iEquipSlotNum = 8;
    for (CNSocket* otherSock : plrv.viewable) {
        otherSock->sendPacket((void*)&pkt, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
    }

    plrv.plr->iPCState = 0;

    sock->sendPacket((void*)&response, P_FE2CL_PC_VEHICLE_OFF_SUCC, sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC));
}

void PlayerManager::setSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH* specialData = (sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, response);

    response.iPC_ID = specialData->iPC_ID;
    response.iReqSpecialStateFlag = specialData->iSpecialStateFlag;
    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC));
}

#pragma region Helper methods
Player *PlayerManager::getPlayer(CNSocket* key) {
    return players[key].plr;
}
#pragma endregion
