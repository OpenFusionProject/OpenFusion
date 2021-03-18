#include "BuiltinCommands.hpp"
#include "PlayerManager.hpp"
#include "Chat.hpp"
#include "Items.hpp"
#include "Missions.hpp"

// helper function, not a packet handler
void BuiltinCommands::setSpecialState(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH))
        return; // ignore the malformed packet

    sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH* setData = (sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // HACK: work around the invisible weapon bug
    if (setData->iSpecialStateFlag == CN_SPECIAL_STATE_FLAG__FULL_UI)
        Items::updateEquips(sock, plr);

    INITSTRUCT(sP_FE2CL_PC_SPECIAL_STATE_CHANGE, response);

    plr->iSpecialState ^= setData->iSpecialStateFlag;

    response.iPC_ID = setData->iPC_ID;
    response.iReqSpecialStateFlag = setData->iSpecialStateFlag;
    response.iSpecialState = plr->iSpecialState;

    sock->sendPacket((void*)&response, P_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC, sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC));
    PlayerManager::sendToViewable(sock, (void*)&response, P_FE2CL_PC_SPECIAL_STATE_CHANGE, sizeof(sP_FE2CL_PC_SPECIAL_STATE_CHANGE));
}

static void setGMSpecialSwitchPlayer(CNSocket* sock, CNPacketData* data) {
    if (PlayerManager::getPlayer(sock)->accountLevel > 30)
        return;

    BuiltinCommands::setSpecialState(sock, data);
}

static void gotoPlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GOTO))
        return; // ignore the malformed packet

    Player *plr = PlayerManager::getPlayer(sock);
    if (plr->accountLevel > 50)
        return;

    sP_CL2FE_REQ_PC_GOTO* gotoData = (sP_CL2FE_REQ_PC_GOTO*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_PC_GOTO_SUCC, response);

    DEBUGLOG(
        std::cout << "P_CL2FE_REQ_PC_GOTO:" << std::endl;
        std::cout << "\tX: " << gotoData->iToX << std::endl;
        std::cout << "\tY: " << gotoData->iToY << std::endl;
        std::cout << "\tZ: " << gotoData->iToZ << std::endl;
        )

    PlayerManager::sendPlayerTo(sock, gotoData->iToX, gotoData->iToY, gotoData->iToZ, INSTANCE_OVERWORLD);
}

static void setValuePlayer(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE))
        return; // ignore the malformed packet

    Player *plr = PlayerManager::getPlayer(sock);
    if (plr->accountLevel > 50)
        return;

    sP_CL2FE_GM_REQ_PC_SET_VALUE* setData = (sP_CL2FE_GM_REQ_PC_SET_VALUE*)data->buf;

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
        // caps
        if (plr->batteryW > 9999)
            plr->batteryW = 9999;
        break;
    case 3:
        plr->batteryN = setData->iSetValue;
        // caps
        if (plr->batteryN > 9999)
            plr->batteryN = 9999;
        break;
    case 4:
        Missions::updateFusionMatter(sock, setData->iSetValue - plr->fusionmatter);
        break;
    case 5:
        plr->money = setData->iSetValue;
        break;
    }

    response.iPC_ID = setData->iPC_ID;
    response.iSetValue = setData->iSetValue;
    response.iSetValueType = setData->iSetValueType;

    sock->sendPacket((void*)&response, P_FE2CL_GM_REP_PC_SET_VALUE, sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE));

    // if one lowers their own health to 0, make sure others can see it
    if (plr->HP <= 0) {
        INITSTRUCT(sP_FE2CL_PC_SUDDEN_DEAD, dead);

        dead.iPC_ID = plr->iID;
        dead.iDamage = plr->HP;
        dead.iHP = plr->HP = 0;

        PlayerManager::sendToViewable(sock, (void*)&dead, P_FE2CL_PC_SUDDEN_DEAD, sizeof(sP_FE2CL_PC_SUDDEN_DEAD));
    }
}

static void setGMSpecialOnOff(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF))
        return; // sanity check

    Player *plr = PlayerManager::getPlayer(sock);

    // access check
    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF *req = (sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF*)data->buf;

    CNSocket *otherSock = PlayerManager::getSockFromAny(req->eTargetSearchBy, req->iTargetPC_ID, req->iTargetPC_UID,
        AUTOU16TOU8(req->szTargetPC_FirstName), AUTOU16TOU8(req->szTargetPC_LastName));
    if (otherSock == nullptr) {
        Chat::sendServerMessage(sock, "player to teleport not found");
        return;
    }

    Player *otherPlr = PlayerManager::getPlayer(otherSock);
    if (req->iONOFF)
        otherPlr->iSpecialState |= req->iSpecialStateFlag;
    else
        otherPlr->iSpecialState &= ~req->iSpecialStateFlag;

    // this is only used for muting players, so no need to update the client since that logic is server-side
}

static void locatePlayer(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_LOCATION))
        return; // sanity check

    Player *plr = PlayerManager::getPlayer(sock);

    // access check
    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_GM_REQ_PC_LOCATION *req = (sP_CL2FE_GM_REQ_PC_LOCATION*)data->buf;

    CNSocket *otherSock = PlayerManager::getSockFromAny(req->eTargetSearchBy, req->iTargetPC_ID, req->iTargetPC_UID,
        AUTOU16TOU8(req->szTargetPC_FirstName), AUTOU16TOU8(req->szTargetPC_LastName));
    if (otherSock == nullptr) {
        Chat::sendServerMessage(sock, "player not found");
        return;
    }

    INITSTRUCT(sP_FE2CL_GM_REP_PC_LOCATION, resp);
    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    resp.iTargetPC_UID = otherPlr->accountId;
    resp.iTargetPC_ID = otherPlr->iID;
    resp.iShardID = 0; // sharding is unsupported
    resp.iMapType = !!PLAYERID(otherPlr->instanceID); // private instance or not
    resp.iMapID = PLAYERID(otherPlr->instanceID);
    resp.iMapNum = MAPNUM(otherPlr->instanceID);
    resp.iX = otherPlr->x;
    resp.iY = otherPlr->y;
    resp.iZ = otherPlr->z;

    memcpy(resp.szTargetPC_FirstName, otherPlr->PCStyle.szFirstName, sizeof(resp.szTargetPC_FirstName));
    memcpy(resp.szTargetPC_LastName, otherPlr->PCStyle.szLastName, sizeof(resp.szTargetPC_LastName));

    sock->sendPacket((void*)&resp, P_FE2CL_GM_REP_PC_LOCATION, sizeof(sP_FE2CL_GM_REP_PC_LOCATION));
}

static void kickPlayer(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_KICK_PLAYER))
        return; // sanity check

    Player *plr = PlayerManager::getPlayer(sock);

    // access check
    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_GM_REQ_KICK_PLAYER *req = (sP_CL2FE_GM_REQ_KICK_PLAYER*)data->buf;

    CNSocket *otherSock = PlayerManager::getSockFromAny(req->eTargetSearchBy, req->iTargetPC_ID, req->iTargetPC_UID,
        AUTOU16TOU8(req->szTargetPC_FirstName), AUTOU16TOU8(req->szTargetPC_LastName));
    if (otherSock == nullptr) {
        Chat::sendServerMessage(sock, "player not found");
        return;
    }

    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    if (plr->accountLevel > otherPlr->accountLevel) {
        Chat::sendServerMessage(sock, "player has higher access level");
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_EXIT_SUCC, response);

    response.iID = otherPlr->iID;
    response.iExitCode = 3; // "a GM has terminated your connection"

    // send to target player
    otherSock->sendPacket((void*)&response, P_FE2CL_REP_PC_EXIT_SUCC, sizeof(sP_FE2CL_REP_PC_EXIT_SUCC));

    // ensure that the connection has terminated
    otherSock->kill();
}

static void warpToPlayer(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_WARP_TO_PC))
        return; // sanity check

    Player *plr = PlayerManager::getPlayer(sock);

    // access check
    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_REQ_PC_WARP_TO_PC *req = (sP_CL2FE_REQ_PC_WARP_TO_PC*)data->buf;

    Player *otherPlr = PlayerManager::getPlayerFromID(req->iPC_ID);
    if (otherPlr == nullptr) {
        Chat::sendServerMessage(sock, "player not found");
        return;
    }

    PlayerManager::sendPlayerTo(sock, otherPlr->x, otherPlr->y, otherPlr->z, otherPlr->instanceID);
}

// GM teleport command
static void teleportPlayer(CNSocket *sock, CNPacketData *data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT))
        return; // sanity check

    Player *plr = PlayerManager::getPlayer(sock);

    // access check
    if (plr->accountLevel > 30)
        return;

    sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT *req = (sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT*)data->buf;

    // player to teleport
    CNSocket *targetSock = PlayerManager::getSockFromAny(req->eTargetPCSearchBy, req->iTargetPC_ID, req->iTargetPC_UID,
        AUTOU16TOU8(req->szTargetPC_FirstName), AUTOU16TOU8(req->szTargetPC_LastName));
    if (targetSock == nullptr) {
        Chat::sendServerMessage(sock, "player to teleport not found");
        return;
    }

    CNSocket *goalSock = nullptr;
    Player *goalPlr = nullptr;
    Player *targetPlr = nullptr;
    uint64_t instance = plr->instanceID;
    const int unstickRange = 400;

    switch (req->eTeleportType) {
    case eCN_GM_TeleportMapType__MyLocation:
        PlayerManager::sendPlayerTo(targetSock, plr->x, plr->y, plr->z, instance);
        break;
    case eCN_GM_TeleportMapType__MapXYZ:
        instance = req->iToMap;
        // fallthrough
    case eCN_GM_TeleportMapType__XYZ:
        PlayerManager::sendPlayerTo(targetSock, req->iToX, req->iToY, req->iToZ, instance);
        break;
    case eCN_GM_TeleportMapType__SomeoneLocation:
        // player to teleport to
        goalSock = PlayerManager::getSockFromAny(req->eGoalPCSearchBy, req->iGoalPC_ID, req->iGoalPC_UID,
            AUTOU16TOU8(req->szGoalPC_FirstName), AUTOU16TOU8(req->szGoalPC_LastName));
        if (goalSock == nullptr) {
            Chat::sendServerMessage(sock, "teleportation target player not found");
            return;
        }
        goalPlr = PlayerManager::getPlayer(goalSock);

        PlayerManager::sendPlayerTo(targetSock, goalPlr->x, goalPlr->y, goalPlr->z, goalPlr->instanceID);
        break;
    case eCN_GM_TeleportMapType__Unstick:
        targetPlr = PlayerManager::getPlayer(targetSock);

        PlayerManager::sendPlayerTo(targetSock, targetPlr->x - unstickRange/2 + rand() % unstickRange,
            targetPlr->y - unstickRange/2 + rand() % unstickRange, targetPlr->z + 80);
        break;
    }
}

static void itemGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM))
        return; // ignore the malformed packet

    sP_CL2FE_REQ_PC_GIVE_ITEM* itemreq = (sP_CL2FE_REQ_PC_GIVE_ITEM*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (plr->accountLevel > 50) {
    	// TODO: send fail packet
        return;
    }

    if (itemreq->eIL == 2) {
        // Quest item, not a real item, handle this later, stubbed for now
        // sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_GIVE_ITEM_FAIL, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL), sock->getFEKey()));
    } else if (itemreq->eIL == 1 && itemreq->Item.iType >= 0 && itemreq->Item.iType <= 10) {

        if (Items::ItemData.find(std::pair<int32_t, int32_t>(itemreq->Item.iID, itemreq->Item.iType)) == Items::ItemData.end()) {
            // invalid item
            std::cout << "[WARN] Item id " << itemreq->Item.iID << " with type " << itemreq->Item.iType << " is invalid (give item)" << std::endl;
            return;
        }

        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = itemreq->eIL;
        resp.iSlotNum = itemreq->iSlotNum;
        if (itemreq->Item.iType == 10) {
            // item is vehicle, set expiration date
            // set time limit: current time + 7days
            itemreq->Item.iTimeLimit = getTimestamp() + 604800;
        }
        resp.Item = itemreq->Item;

        plr->Inven[itemreq->iSlotNum] = itemreq->Item;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
    }
}

void BuiltinCommands::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GOTO, gotoPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SET_VALUE, setValuePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_ITEM, itemGMGiveHandler);

    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH, setGMSpecialSwitchPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF, setGMSpecialOnOff);

    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_LOCATION, locatePlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_KICK_PLAYER, kickPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_WARP_TO_PC, warpToPlayer);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_TARGET_PC_TELEPORT, teleportPlayer);
}
