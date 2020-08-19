#include "CNProtocol.hpp"
#include "PlayerManager.hpp"
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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_MOVEPLATFORM, PlayerManager::movePlatformPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GOTO, PlayerManager::gotoPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SET_VALUE, PlayerManager::setSpecialPlayer);
}

void PlayerManager::addPlayer(CNSocket* key, Player plr) {
    players[key] = PlayerView();
    players[key].viewable = std::list<CNSocket*>();
    players[key].plr = plr;

    std::cout << U16toU8(plr.PCStyle.szFirstName) << U16toU8(plr.PCStyle.szLastName) << " has joined!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

void PlayerManager::removePlayer(CNSocket* key) {
    PlayerView cachedView = players[key];

    // if players have them in their viewable lists, remove it
    for (CNSocket* otherSock : players[key].viewable) {
        players[otherSock].viewable.remove(key); // gone

        // now sent PC_EXIT packet
        sP_FE2CL_PC_EXIT* exitPacket = (sP_FE2CL_PC_EXIT*)xmalloc(sizeof(sP_FE2CL_PC_EXIT));
        exitPacket->iID = players[key].plr.iID;

        otherSock->sendPacket(new CNPacketData((void*)exitPacket, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT), otherSock->getFEKey()));
    }

    players.erase(key);

    std::cout << U16toU8(cachedView.plr.PCStyle.szFirstName) << U16toU8(cachedView.plr.PCStyle.szLastName) << " has left!" << std::endl;
    std::cout << players.size() << " players" << std::endl;
}

Player PlayerManager::getPlayer(CNSocket* key) {
    return players[key].plr;
}

void PlayerManager::updatePlayerPosition(CNSocket* sock, int X, int Y, int Z) {
    players[sock].plr.x = X;
    players[sock].plr.y = Y;
    players[sock].plr.z = Z;

    std::vector<CNSocket*> noView;
    std::vector<CNSocket*> yesView;

    // TODO: oh god this is sooooo perfomance heavy the more players you have
    for (auto pair : players) {
        if (pair.first == sock)
            continue; // ignore our own connection

        int diffX = abs(pair.second.plr.x - X); // the map is like a grid, X and Y are your position on the map, Z is the height. very different from other games...
        int diffY = abs(pair.second.plr.y - Y);

        double dist = sqrt(pow(diffX, 2) + pow(diffY, 2));

        if (dist > settings::VIEWDISTANCE) {
            noView.push_back(pair.first);
        } else {
            yesView.push_back(pair.first);
        }
    }

    std::list<CNSocket*> cachedview(players[sock].viewable); // copies the viewable

    for (CNSocket* otherSock : cachedview) {
        if (std::find(noView.begin(), noView.end(), otherSock) != noView.end()) {
            // sock shouldn't be visible, send PC_EXIT packet & remove them

            sP_FE2CL_PC_EXIT* exitPacket = (sP_FE2CL_PC_EXIT*)xmalloc(sizeof(sP_FE2CL_PC_EXIT));
            sP_FE2CL_PC_EXIT* exitPacketOther = (sP_FE2CL_PC_EXIT*)xmalloc(sizeof(sP_FE2CL_PC_EXIT));
            
            exitPacket->iID = players[sock].plr.iID;
            exitPacketOther->iID = players[otherSock].plr.iID;

            otherSock->sendPacket(new CNPacketData((void*)exitPacket, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT), otherSock->getFEKey()));
            sock->sendPacket(new CNPacketData((void*)exitPacketOther, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT), sock->getFEKey()));

            players[sock].viewable.remove(otherSock);
            players[otherSock].viewable.remove(sock);
        }
    }

    cachedview = players[sock].viewable;

    for (CNSocket* otherSock : yesView) {
        if (std::find(cachedview.begin(), cachedview.end(), otherSock) == cachedview.end()) {
            // this needs to be added to the viewable players, send PC_ENTER

            sP_FE2CL_PC_NEW* newPlayer = (sP_FE2CL_PC_NEW*)xmalloc(sizeof(sP_FE2CL_PC_NEW)); // current connection to other player
            sP_FE2CL_PC_NEW* newOtherPlayer = (sP_FE2CL_PC_NEW*)xmalloc(sizeof(sP_FE2CL_PC_NEW)); // other player to current connection

            Player otherPlr = players[otherSock].plr;
            Player plr = players[sock].plr;

            newPlayer->PCAppearanceData.iID = plr.iID;
            newPlayer->PCAppearanceData.iHP = plr.HP;
            newPlayer->PCAppearanceData.iLv = plr.level;
            newPlayer->PCAppearanceData.iX = plr.x;
            newPlayer->PCAppearanceData.iY = plr.y;
            newPlayer->PCAppearanceData.iZ = plr.z;
            newPlayer->PCAppearanceData.iAngle = plr.angle;
            newPlayer->PCAppearanceData.PCStyle = plr.PCStyle;
            memcpy(newPlayer->PCAppearanceData.ItemEquip, plr.Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            newOtherPlayer->PCAppearanceData.iID = otherPlr.iID;
            newOtherPlayer->PCAppearanceData.iHP = otherPlr.HP;
            newOtherPlayer->PCAppearanceData.iLv = otherPlr.level;
            newOtherPlayer->PCAppearanceData.iX = otherPlr.x;
            newOtherPlayer->PCAppearanceData.iY = otherPlr.y;
            newOtherPlayer->PCAppearanceData.iZ = otherPlr.z;
            newOtherPlayer->PCAppearanceData.iAngle = otherPlr.angle;
            newOtherPlayer->PCAppearanceData.PCStyle = otherPlr.PCStyle;
            memcpy(newOtherPlayer->PCAppearanceData.ItemEquip, otherPlr.Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            sock->sendPacket(new CNPacketData((void*)newOtherPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW), sock->getFEKey()));
            otherSock->sendPacket(new CNPacketData((void*)newPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW), otherSock->getFEKey()));

            players[sock].viewable.push_back(otherSock);
            players[otherSock].viewable.push_back(sock);
        }
    }
}

void PlayerManager::enterPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_ENTER))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_ENTER* enter = (sP_CL2FE_REQ_PC_ENTER*)data->buf;
    sP_FE2CL_REP_PC_ENTER_SUCC* response = (sP_FE2CL_REP_PC_ENTER_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_ENTER_SUCC));

    // TODO: check if serialkey exists, if it doesn't send sP_FE2CL_REP_PC_ENTER_FAIL
    Player plr = CNSharedData::getPlayer(enter->iEnterSerialKey);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_ENTER:" << std::endl;
        std::cout << "\tID: " << U16toU8(enter->szID) << std::endl;
        std::cout << "\tSerial: " << enter->iEnterSerialKey << std::endl;
        std::cout << "\tTemp: " << enter->iTempValue << std::endl;
        std::cout << "\tPC_UID: " << plr.PCStyle.iPC_UID << std::endl;
    )

    response->iID = rand();
    response->uiSvrTime = getTime();
    response->PCLoadData2CL.iUserLevel = 1;
    response->PCLoadData2CL.iHP = 1000 * plr.level;
    response->PCLoadData2CL.iLevel = plr.level;
    response->PCLoadData2CL.iMentor = 1;
    response->PCLoadData2CL.iMentorCount = 4;
    response->PCLoadData2CL.iMapNum = 0;
    response->PCLoadData2CL.iX = plr.x;
    response->PCLoadData2CL.iY = plr.y;
    response->PCLoadData2CL.iZ = plr.z;
    response->PCLoadData2CL.iActiveNanoSlotNum = -1;
    response->PCLoadData2CL.iFatigue = 50;
    response->PCLoadData2CL.PCStyle = plr.PCStyle;
    response->PCLoadData2CL.PCStyle2 = plr.PCStyle2;

    for (int i = 0; i < AEQUIP_COUNT; i++)
        response->PCLoadData2CL.aEquip[i] = plr.Equip[i];

    // don't ask..
    for (int i = 1; i < 37; i++) {
        response->PCLoadData2CL.aNanoBank[i].iID = i;
        response->PCLoadData2CL.aNanoBank[i].iSkillID = 1;
        response->PCLoadData2CL.aNanoBank[i].iStamina = 150;
    }

    response->PCLoadData2CL.aNanoSlots[0] = 1;
    response->PCLoadData2CL.aNanoSlots[1] = 2;
    response->PCLoadData2CL.aNanoSlots[2] = 3;

    response->PCLoadData2CL.aQuestFlag[0] = -1;

    plr.iID = response->iID;
    plr.SerialKey = enter->iEnterSerialKey;
    plr.HP = response->PCLoadData2CL.iHP;

    sock->setEKey(CNSocketEncryption::createNewKey(response->uiSvrTime, response->iID + 1, response->PCLoadData2CL.iFusionMatter + 1));
    sock->setFEKey(plr.FEKey);

    sock->sendPacket(new CNPacketData((void*)response, P_FE2CL_REP_PC_ENTER_SUCC, sizeof(sP_FE2CL_REP_PC_ENTER_SUCC), sock->getFEKey()));

    addPlayer(sock, plr);
}

void PlayerManager::loadPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_LOADING_COMPLETE))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_LOADING_COMPLETE* complete = (sP_CL2FE_REQ_PC_LOADING_COMPLETE*)data->buf;
    sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC* response = (sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC));

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_LOADING_COMPLETE:" << std::endl;
        std::cout << "\tPC_ID: " << complete->iPC_ID << std::endl; 
    )

    response->iPC_ID = complete->iPC_ID;

    sock->sendPacket(new CNPacketData((void*)response, P_FE2CL_REP_PC_LOADING_COMPLETE_SUCC, sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC), sock->getFEKey()));
}

void PlayerManager::movePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVE))
        return; // ignore the malformed packet
    
    sP_CL2FE_REQ_PC_MOVE* moveData = (sP_CL2FE_REQ_PC_MOVE*)data->buf;
    updatePlayerPosition(sock, moveData->iX, moveData->iY, moveData->iZ);

    players[sock].plr.angle = moveData->iAngle;
    uint64_t tm = getTime();

    for (CNSocket* otherSock : players[sock].viewable) {
        sP_FE2CL_PC_MOVE* moveResponse = (sP_FE2CL_PC_MOVE*)xmalloc(sizeof(sP_FE2CL_PC_MOVE));

        moveResponse->iID = players[sock].plr.iID;
        moveResponse->cKeyValue = moveData->cKeyValue;

        moveResponse->iX = moveData->iX;
        moveResponse->iY = moveData->iY;
        moveResponse->iZ = moveData->iZ;
        moveResponse->iAngle = moveData->iAngle;
        moveResponse->fVX = moveData->fVX;
        moveResponse->fVY = moveData->fVY;
        moveResponse->fVZ = moveData->fVZ;
        
        moveResponse->iSpeed = moveData->iSpeed;
        moveResponse->iCliTime = moveData->iCliTime; // maybe don't send this??? seems unneeded...
        moveResponse->iSvrTime = tm;

        otherSock->sendPacket(new CNPacketData((void*)moveResponse, P_FE2CL_PC_MOVE, sizeof(sP_FE2CL_PC_MOVE), otherSock->getFEKey()));
    }
}

void PlayerManager::stopPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_STOP))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_STOP* stopData = (sP_CL2FE_REQ_PC_STOP*)data->buf;
    updatePlayerPosition(sock, stopData->iX, stopData->iY, stopData->iZ);

    uint64_t tm = getTime();

    for (CNSocket* otherSock : players[sock].viewable) {
        sP_FE2CL_PC_STOP* stopResponse = (sP_FE2CL_PC_STOP*)xmalloc(sizeof(sP_FE2CL_PC_STOP));

        stopResponse->iID = players[sock].plr.iID;

        stopResponse->iX = stopData->iX;
        stopResponse->iY = stopData->iY;
        stopResponse->iZ = stopData->iZ;

        stopResponse->iCliTime = stopData->iCliTime; // maybe don't send this??? seems unneeded...
        stopResponse->iSvrTime = tm;

        otherSock->sendPacket(new CNPacketData((void*)stopResponse, P_FE2CL_PC_STOP, sizeof(sP_FE2CL_PC_STOP), otherSock->getFEKey()));
    }
}

void PlayerManager::jumpPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_JUMP))
        return; // ignore the malformed packet
    
    sP_CL2FE_REQ_PC_JUMP* jumpData = (sP_CL2FE_REQ_PC_JUMP*)data->buf;
    updatePlayerPosition(sock, jumpData->iX, jumpData->iY, jumpData->iZ);

    uint64_t tm = getTime();

    for (CNSocket* otherSock : players[sock].viewable) {
        sP_FE2CL_PC_JUMP* jumpResponse = (sP_FE2CL_PC_JUMP*)xmalloc(sizeof(sP_FE2CL_PC_JUMP));

        jumpResponse->iID = players[sock].plr.iID;
        jumpResponse->cKeyValue = jumpData->cKeyValue;

        jumpResponse->iX = jumpData->iX;
        jumpResponse->iY = jumpData->iY;
        jumpResponse->iZ = jumpData->iZ;
        jumpResponse->iAngle = jumpData->iAngle;
        jumpResponse->iVX = jumpData->iVX;
        jumpResponse->iVY = jumpData->iVY;
        jumpResponse->iVZ = jumpData->iVZ;
        
        jumpResponse->iSpeed = jumpData->iSpeed;
        jumpResponse->iCliTime = jumpData->iCliTime; // maybe don't send this??? seems unneeded...
        jumpResponse->iSvrTime = tm;

        otherSock->sendPacket(new CNPacketData((void*)jumpResponse, P_FE2CL_PC_JUMP, sizeof(sP_FE2CL_PC_JUMP), otherSock->getFEKey()));
    }
}

void PlayerManager::movePlatformPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_MOVEPLATFORM* platformData = (sP_CL2FE_REQ_PC_MOVEPLATFORM*)data->buf;
    updatePlayerPosition(sock, platformData->iX, platformData->iY, platformData->iZ);

    uint64_t tm = getTime();

    for (CNSocket* otherSock : players[sock].viewable) {

        sP_FE2CL_PC_MOVEPLATFORM* platResponse = (sP_FE2CL_PC_MOVEPLATFORM*)xmalloc(sizeof(sP_FE2CL_PC_MOVEPLATFORM));

        platResponse->iPC_ID = players[sock].plr.iID;
        platResponse->iCliTime = platformData->iCliTime;
        platResponse->iSvrTime = tm;
        platResponse->iX = platformData->iX;
        platResponse->iY = platformData->iY;
        platResponse->iZ = platformData->iZ;
        platResponse->iAngle = platformData->iAngle;
        platResponse->fVX = platformData->fVX;
        platResponse->fVY = platformData->fVY;
        platResponse->fVZ = platformData->fVZ;
        platResponse->iLcX = platformData->iLcX;
        platResponse->iLcY = platformData->iLcY;
        platResponse->iLcZ = platformData->iLcZ;
        platResponse->iSpeed = platformData->iSpeed;
        platResponse->bDown = platformData->bDown;
        platResponse->cKeyValue = platformData->cKeyValue;
        platResponse->iPlatformID = platformData->iPlatformID;

        otherSock->sendPacket(new CNPacketData((void*)platResponse, P_FE2CL_PC_MOVEPLATFORM, sizeof(sP_FE2CL_PC_MOVEPLATFORM), otherSock->getFEKey()));
    }
}

void PlayerManager::gotoPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GOTO))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GOTO* gotoData = (sP_CL2FE_REQ_PC_GOTO*)data->buf;
    sP_FE2CL_REP_PC_GOTO_SUCC* response = (sP_FE2CL_REP_PC_GOTO_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_PC_GOTO_SUCC));

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_GOTO:" << std::endl;
        std::cout << "\tX: " << gotoData->iToX << std::endl;
        std::cout << "\tY: " << gotoData->iToY << std::endl;
        std::cout << "\tZ: " << gotoData->iToZ << std::endl;
    )

    response->iX = gotoData->iToX;
    response->iY = gotoData->iToY;
    response->iZ = gotoData->iToZ;

    sock->sendPacket(new CNPacketData((void*)response, P_FE2CL_REP_PC_GOTO_SUCC, sizeof(sP_FE2CL_REP_PC_GOTO_SUCC), sock->getFEKey()));
}

void PlayerManager::setSpecialPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE))
        return; // ignore the malformed packet

    sP_CL2FE_GM_REQ_PC_SET_VALUE* setData = (sP_CL2FE_GM_REQ_PC_SET_VALUE*)data->buf;
    sP_FE2CL_GM_REP_PC_SET_VALUE* response = (sP_FE2CL_GM_REP_PC_SET_VALUE*)xmalloc(sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));

    DEBUGLOG(
        std::cout << "P_CL2FE_GM_REQ_PC_SET_VALUE:" << std::endl;
        std::cout << "\tPC_ID: " << setData->iPC_ID << std::endl;
        std::cout << "\tSetValueType: " << setData->iSetValueType << std::endl;
        std::cout << "\tSetValue: " << setData->iSetValue << std::endl;
    )

    response->iPC_ID = setData->iPC_ID;
    response->iSetValue = setData->iSetValue;
    response->iSetValueType = setData->iSetValueType;

    sock->sendPacket(new CNPacketData((void*)response, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE), sock->getFEKey()));
}