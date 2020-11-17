#include "CNLoginServer.hpp"
#include "CNShared.hpp"
#include "CNStructs.hpp"
#include "Database.hpp"
#include "PlayerManager.hpp"
#include <regex>
#include "contrib/bcrypt/BCrypt.hpp"

#include "settings.hpp"

std::map<CNSocket*, CNLoginData> CNLoginServer::loginSessions;

CNLoginServer::CNLoginServer(uint16_t p) {
    port = p;
    pHandler = &CNLoginServer::handlePacket;
    init();
}

void CNLoginServer::handlePacket(CNSocket* sock, CNPacketData* data) {
    printPacket(data, CL2LS);

    switch (data->type) {
        case P_CL2LS_REQ_LOGIN: {
            login(sock, data);
            break;
        }
        case P_CL2LS_REP_LIVE_CHECK: {
            loginSessions[sock].lastHeartbeat = getTime();
            break;
        }
        case P_CL2LS_REQ_CHECK_CHAR_NAME: {
            nameCheck(sock, data);
            break;
        }
        case P_CL2LS_REQ_SAVE_CHAR_NAME: {
            nameSave(sock, data);
            break;
        }
        case P_CL2LS_REQ_CHAR_CREATE: {
            characterCreate(sock, data);
            break;
        }
        case P_CL2LS_REQ_CHAR_DELETE: {
            characterDelete(sock, data);
            break;
        }
        case P_CL2LS_REQ_CHAR_SELECT: {
            characterSelect(sock, data);
            break;
        }
        case P_CL2LS_REQ_SHARD_SELECT: {
            shardSelect(sock, data);
            break;
        }
        case P_CL2LS_REQ_SAVE_CHAR_TUTOR: {
            finishTutorial(sock, data);
            break;
        }
        case P_CL2LS_REQ_CHANGE_CHAR_NAME: {
            changeName(sock, data);
            break;
        }
        case P_CL2LS_REQ_PC_EXIT_DUPLICATE:{
            duplicateExit(sock, data);
            break;
        }
        default:
            if (settings::VERBOSITY)
                std::cerr << "OpenFusion: LOGIN UNIMPLM ERR. PacketType: " << Defines::p2str(CL2LS, data->type) << " (" << data->type << ")" << std::endl;
            break;
        /*
         * Unimplemented CL2LS packets:
         *  P_CL2LS_CHECK_NAME_LIST - unused by the client
         *  P_CL2LS_REQ_SERVER_SELECT
         *  P_CL2LS_REQ_SHARD_LIST_INFO - dev commands, useless as we only run 1 server
         */
    }
}

#pragma region packets

void loginFail(LoginError errorCode, std::string userLogin, CNSocket* sock) {
    INITSTRUCT(sP_LS2CL_REP_LOGIN_FAIL, resp);
    U8toU16(userLogin, resp.szID, sizeof(resp.szID));
    resp.iErrorCode = (int)errorCode;
    sock->sendPacket((void*)&resp, P_LS2CL_REP_LOGIN_FAIL, sizeof(sP_LS2CL_REP_LOGIN_FAIL));

    DEBUGLOG(
        std::cout << "Login Server: Login fail. Error code " << (int)errorCode << std::endl;
    )

    return;
}


void CNLoginServer::login(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_LOGIN))
        return; // ignore the malformed packet

    sP_CL2LS_REQ_LOGIN* login = (sP_CL2LS_REQ_LOGIN*)data->buf;
    // TODO: implement better way of sending credentials
    std::string userLogin((char*)login->szCookie_TEGid);
    std::string userPassword((char*)login->szCookie_authid);

    /*
     * Sometimes the client sends garbage cookie data.
     * Validate it as normal credentials instead of using a length check before falling back.
     */
    if (!CNLoginServer::isLoginDataGood(userLogin, userPassword)) {
        /*
         * The std::string -> char* -> std::string maneuver should remove any
         * trailing garbage after the null terminator.
         */
        userLogin = std::string(U16toU8(login->szID).c_str());
        userPassword = std::string(U16toU8(login->szPassword).c_str());
    }

    // check regex
    if (!CNLoginServer::isLoginDataGood(userLogin, userPassword))
        return loginFail(LoginError::LOGIN_ERROR, userLogin, sock);

    std::unique_ptr<Database::Account> findUser = Database::findAccount(userLogin);
    if (findUser == nullptr)
        return newAccount(sock, userLogin, userPassword, login->iClientVerC);

    if (!CNLoginServer::isPasswordCorrect(findUser->Password, userPassword))
        return loginFail(LoginError::ID_AND_PASSWORD_DO_NOT_MATCH, userLogin, sock);

    /* calling this here to timestamp login attempt,
     * in order to make duplicate exit sanity check work*/
    Database::updateSelected(findUser->AccountID, findUser->Selected);

    if (CNLoginServer::isAccountInUse(findUser->AccountID))
        return loginFail(LoginError::ID_ALREADY_IN_USE, userLogin, sock);

    loginSessions[sock] = CNLoginData();
    loginSessions[sock].userID = findUser->AccountID;
    loginSessions[sock].slot = findUser->Selected;
    loginSessions[sock].lastHeartbeat = getTime();

    std::vector<Player> characters = Database::getCharacters(loginSessions[sock].userID);
    int charCount = characters.size();

    INITSTRUCT(sP_LS2CL_REP_LOGIN_SUCC, resp);
    memcpy(resp.szID, login->szID, sizeof(char16_t) * 33);

    resp.iCharCount = charCount;
    resp.iSlotNum = loginSessions[sock].slot;
    resp.iPaymentFlag = 1;
    resp.iOpenBetaFlag = 0;
    resp.uiSvrTime = getTime();

    // send the resp in with original key
    sock->sendPacket((void*)&resp, P_LS2CL_REP_LOGIN_SUCC, sizeof(sP_LS2CL_REP_LOGIN_SUCC));

    // update keys
    sock->setEKey(CNSocketEncryption::createNewKey(resp.uiSvrTime, resp.iCharCount + 1, resp.iSlotNum + 1));
    sock->setFEKey(CNSocketEncryption::createNewKey((uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]), login->iClientVerC, 1));

    // now send the characters :)
    std::vector<Player>::iterator it;
    for (it = characters.begin(); it != characters.end(); it++) {
        sP_LS2CL_REP_CHAR_INFO charInfo = sP_LS2CL_REP_CHAR_INFO();

        charInfo.iSlot = (int8_t)it->slot;
        charInfo.iLevel = (int16_t)it->level;
        charInfo.sPC_Style = it->PCStyle;
        charInfo.sPC_Style2 = it->PCStyle2;

        // position
        charInfo.iX = it->x;
        charInfo.iY = it->y;
        charInfo.iZ = it->z;

        // save character in session (for char select)
        int UID = it->iID;
        loginSessions[sock].characters[UID] = Player(*it);
        loginSessions[sock].characters[UID].FEKey = sock->getFEKey();

        // Equip info
        for (int i = 0; i < AEQUIP_COUNT; i++) {
            charInfo.aEquip[i] = it->Equip[i];
        }

        // set default to the first character
        if (it == characters.begin())
            loginSessions[sock].selectedChar = UID;

        sock->sendPacket((void*)&charInfo, P_LS2CL_REP_CHAR_INFO, sizeof(sP_LS2CL_REP_CHAR_INFO));
    }

    DEBUGLOG(
        std::cout << "Login Server: Login success. Welcome " << userLogin << " [" << loginSessions[sock].userID << "]" << std::endl;
    )
}

void CNLoginServer::newAccount(CNSocket* sock, std::string userLogin, std::string userPassword, int32_t clientVerC) {   
    loginSessions[sock] = CNLoginData();
    loginSessions[sock].userID = Database::addAccount(userLogin, userPassword);
    loginSessions[sock].slot = 1;
    loginSessions[sock].lastHeartbeat = getTime();

    INITSTRUCT(sP_LS2CL_REP_LOGIN_SUCC, resp);
    U8toU16(userLogin, resp.szID, sizeof(resp.szID));

    resp.iCharCount = 0;
    resp.iSlotNum = loginSessions[sock].slot;
    resp.iPaymentFlag = 1;
    resp.iOpenBetaFlag = 0;
    resp.uiSvrTime = getTime();

    // send the resp in with original key
    sock->sendPacket((void*)&resp, P_LS2CL_REP_LOGIN_SUCC, sizeof(sP_LS2CL_REP_LOGIN_SUCC));

    // update keys
    sock->setEKey(CNSocketEncryption::createNewKey(resp.uiSvrTime, resp.iCharCount + 1, resp.iSlotNum + 1));
    sock->setFEKey(CNSocketEncryption::createNewKey((uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]), clientVerC, 1));

    DEBUGLOG(
        std::cout << "Login Server: New account. Welcome " << userLogin << " [" << loginSessions[sock].userID << "]" << std::endl;
    )
}

void CNLoginServer::nameCheck(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_CHECK_CHAR_NAME))
        return;

    sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck = (sP_CL2LS_REQ_CHECK_CHAR_NAME*)data->buf;
    bool success = true;
    int errorcode = 0;

    // check regex
    if (!CNLoginServer::isCharacterNameGood(U16toU8(nameCheck->szFirstName), U16toU8(nameCheck->szLastName))) {
        success = false;
        errorcode = 4;
    }
    else if (!Database::isNameFree(nameCheck)) { // check if name isn't already occupied
        success = false;
        errorcode = 1;
    }

    loginSessions[sock].lastHeartbeat = getTime();

    if (success) {
        INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC, resp);

        DEBUGLOG(
            std::cout << "P_CL2LS_REQ_CHECK_CHAR_NAME:" << std::endl;
        std::cout << "\tFirstName: " << U16toU8(nameCheck->szFirstName) << " LastName: " << U16toU8(nameCheck->szLastName) << std::endl;
        )

            memcpy(resp.szFirstName, nameCheck->szFirstName, sizeof(char16_t) * 9);
        memcpy(resp.szLastName, nameCheck->szLastName, sizeof(char16_t) * 17);

        sock->sendPacket((void*)&resp, P_LS2CL_REP_CHECK_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC));
    }
    else {
        INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL, resp);
        resp.iErrorCode = errorcode;
        sock->sendPacket((void*)&resp, P_LS2CL_REP_CHECK_CHAR_NAME_FAIL, sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL));
    }
}

void CNLoginServer::nameSave(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_SAVE_CHAR_NAME))
        return;

    sP_CL2LS_REQ_SAVE_CHAR_NAME* save = (sP_CL2LS_REQ_SAVE_CHAR_NAME*)data->buf;
    INITSTRUCT(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC, resp);

    DEBUGLOG(
        std::cout << "P_CL2LS_REQ_SAVE_CHAR_NAME:" << std::endl;
    std::cout << "\tSlot: " << (int)save->iSlotNum << std::endl;
    std::cout << "\tName: " << U16toU8(save->szFirstName) << " " << U16toU8(save->szLastName) << std::endl;
    )

    resp.iSlotNum = save->iSlotNum;
    resp.iGender = save->iGender;
    resp.iPC_UID = Database::createCharacter(save, loginSessions[sock].userID);
    memcpy(resp.szFirstName, save->szFirstName, sizeof(char16_t) * 9);
    memcpy(resp.szLastName, save->szLastName, sizeof(char16_t) * 17);

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket((void*)&resp, P_LS2CL_REP_SAVE_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC));

    Database::updateSelected(loginSessions[sock].userID, save->iSlotNum);
}

void CNLoginServer::characterCreate(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_CHAR_CREATE))
        return;

    sP_CL2LS_REQ_CHAR_CREATE* character = (sP_CL2LS_REQ_CHAR_CREATE*)data->buf;

    if (!(Database::validateCharacter(character->PCStyle.iPC_UID, loginSessions[sock].userID) ||
        loginSessions[sock].characters.find(character->PCStyle.iPC_UID) == loginSessions[sock].characters.end())) {
        INITSTRUCT(sP_LS2CL_REP_SHARD_SELECT_FAIL, fail);
        fail.iErrorCode = 2;
        sock->sendPacket((void*)&fail, P_LS2CL_REP_SHARD_SELECT_FAIL, sizeof(sP_LS2CL_REP_SHARD_SELECT_FAIL));
        return;
    }

    Database::finishCharacter(character);

    DEBUGLOG(
        std::cout << "P_CL2LS_REQ_CHAR_CREATE:" << std::endl;
    std::cout << "\tPC_UID: " << character->PCStyle.iPC_UID << std::endl;
    std::cout << "\tNameCheck: " << (int)character->PCStyle.iNameCheck << std::endl;
    std::cout << "\tName: " << U16toU8(character->PCStyle.szFirstName) << " " << U16toU8(character->PCStyle.szLastName) << std::endl;
    std::cout << "\tGender: " << (int)character->PCStyle.iGender << std::endl;
    std::cout << "\tFace: " << (int)character->PCStyle.iFaceStyle << std::endl;
    std::cout << "\tHair: " << (int)character->PCStyle.iHairStyle << std::endl;
    std::cout << "\tHair Color: " << (int)character->PCStyle.iHairColor << std::endl;
    std::cout << "\tSkin Color: " << (int)character->PCStyle.iSkinColor << std::endl;
    std::cout << "\tEye Color: " << (int)character->PCStyle.iEyeColor << std::endl;
    std::cout << "\tHeight: " << (int)character->PCStyle.iHeight << std::endl;
    std::cout << "\tBody: " << (int)character->PCStyle.iBody << std::endl;
    std::cout << "\tClass: " << (int)character->PCStyle.iClass << std::endl;
    std::cout << "\tiEquipUBID: " << (int)character->sOn_Item.iEquipUBID << std::endl;
    std::cout << "\tiEquipLBID: " << (int)character->sOn_Item.iEquipLBID << std::endl;
    std::cout << "\tiEquipFootID: " << (int)character->sOn_Item.iEquipFootID << std::endl;
    )

    Player player = Database::getPlayer(character->PCStyle.iPC_UID);
    int64_t UID = player.iID;

    INITSTRUCT(sP_LS2CL_REP_CHAR_CREATE_SUCC, resp);
    resp.sPC_Style = player.PCStyle;
    resp.sPC_Style2 = player.PCStyle2;
    resp.iLevel = player.level;
    resp.sOn_Item = character->sOn_Item;

    // save player in session
    loginSessions[sock].characters[UID] = Player(player);
    loginSessions[sock].characters[UID].FEKey = sock->getFEKey();

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket((void*)&resp, P_LS2CL_REP_CHAR_CREATE_SUCC, sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC));
    Database::updateSelected(loginSessions[sock].userID, player.slot);   
}

void CNLoginServer::characterDelete(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_CHAR_DELETE))
        return;

    sP_CL2LS_REQ_CHAR_DELETE* del = (sP_CL2LS_REQ_CHAR_DELETE*)data->buf;
    int operationResult = Database::deleteCharacter(del->iPC_UID, loginSessions[sock].userID);

    INITSTRUCT(sP_LS2CL_REP_CHAR_DELETE_SUCC, resp);
    resp.iSlotNum = operationResult;
    sock->sendPacket((void*)&resp, P_LS2CL_REP_CHAR_DELETE_SUCC, sizeof(sP_LS2CL_REP_CHAR_DELETE_SUCC));
    loginSessions[sock].lastHeartbeat = getTime();
    loginSessions[sock].characters.erase(del->iPC_UID);
}

void CNLoginServer::characterSelect(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_CHAR_SELECT))
        return;

    // character selected
    sP_CL2LS_REQ_CHAR_SELECT* character = (sP_CL2LS_REQ_CHAR_SELECT*)data->buf;
    INITSTRUCT(sP_LS2CL_REP_CHAR_SELECT_SUCC, resp);

    if (!(Database::validateCharacter(character->iPC_UID, loginSessions[sock].userID) ||
        loginSessions[sock].characters.find(character->iPC_UID) == loginSessions[sock].characters.end())) {
        INITSTRUCT(sP_LS2CL_REP_SHARD_SELECT_FAIL, fail);
        fail.iErrorCode = 2;
        sock->sendPacket((void*)&fail, P_LS2CL_REP_SHARD_SELECT_FAIL, sizeof(sP_LS2CL_REP_SHARD_SELECT_FAIL));
        return;
    }

    DEBUGLOG(
    std::cout << "P_CL2LS_REQ_CHAR_SELECT:" << std::endl;
    std::cout << "\tPC_UID: " << character->iPC_UID << std::endl;
    )

    loginSessions[sock].lastHeartbeat = getTime();

    loginSessions[sock].selectedChar = character->iPC_UID;
    Database::updateSelected(loginSessions[sock].userID, loginSessions[sock].characters[character->iPC_UID].slot);
    sock->sendPacket((void*)&resp, P_LS2CL_REP_CHAR_SELECT_SUCC, sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC));
}

void CNLoginServer::shardSelect(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_SHARD_SELECT))
        return;

    // tell client to connect to the shard server
    sP_CL2LS_REQ_SHARD_SELECT* shard = (sP_CL2LS_REQ_SHARD_SELECT*)data->buf;
    INITSTRUCT(sP_LS2CL_REP_SHARD_SELECT_SUCC, resp);
    
    DEBUGLOG(
        std::cout << "P_CL2LS_REQ_SHARD_SELECT:" << std::endl;
    std::cout << "\tShard: " << (int)shard->ShardNum << std::endl;
    )

    const char* SHARD_IP = settings::SHARDSERVERIP.c_str();
    resp.iEnterSerialKey = rand();
    resp.g_FE_ServerPort = settings::SHARDPORT;

    // copy IP to resp (this struct uses ASCII encoding so we don't have to goof around converting encodings)
    memcpy(resp.g_FE_ServerIP, SHARD_IP, strlen(SHARD_IP));
    resp.g_FE_ServerIP[strlen(SHARD_IP)] = '\0';
    
    // pass player to CNSharedData
    CNSharedData::setPlayer(resp.iEnterSerialKey, loginSessions[sock].characters[loginSessions[sock].selectedChar]);

    sock->sendPacket((void*)&resp, P_LS2CL_REP_SHARD_SELECT_SUCC, sizeof(sP_LS2CL_REP_SHARD_SELECT_SUCC));
}

void CNLoginServer::finishTutorial(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_SAVE_CHAR_TUTOR))
        return;
    sP_CL2LS_REQ_SAVE_CHAR_TUTOR* save = (sP_CL2LS_REQ_SAVE_CHAR_TUTOR*)data->buf;
    Database::finishTutorial(save->iPC_UID);
    // update character in session
    auto key = loginSessions[sock].characters[save->iPC_UID].FEKey;
    loginSessions[sock].characters[save->iPC_UID] = Player(Database::getPlayer(save->iPC_UID));
    loginSessions[sock].characters[save->iPC_UID].FEKey = key;

    loginSessions[sock].lastHeartbeat = getTime();
    // no response here
}

void CNLoginServer::changeName(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_CHANGE_CHAR_NAME))
        return;

    sP_CL2LS_REQ_CHANGE_CHAR_NAME* save = (sP_CL2LS_REQ_CHANGE_CHAR_NAME*)data->buf;
    Database::changeName(save);

    INITSTRUCT(sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC, resp);
    resp.iPC_UID = save->iPCUID;
    memcpy(resp.szFirstName, save->szFirstName, sizeof(char16_t) * 9);
    memcpy(resp.szLastName, save->szLastName, sizeof(char16_t) * 17);
    resp.iSlotNum = save->iSlotNum;

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket((void*)&resp, P_LS2CL_REP_CHANGE_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC));
}

void CNLoginServer::duplicateExit(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2LS_REQ_PC_EXIT_DUPLICATE))
        return;

    sP_CL2LS_REQ_PC_EXIT_DUPLICATE* exit = (sP_CL2LS_REQ_PC_EXIT_DUPLICATE*)data->buf;
    auto account = Database::findAccount(U16toU8(exit->szID));

    // sanity check
    if (account == nullptr) {
        std::cout << "[WARN] P_CL2LS_REQ_PC_EXIT_DUPLICATE submitted unknown username: " << exit->szID << std::endl;
        return;
    }

    exitDuplicate(account->AccountID);
}
#pragma endregion

#pragma region connections
void CNLoginServer::newConnection(CNSocket* cns) {
    cns->setActiveKey(SOCKETKEY_E); // by default they accept keys encrypted with the default key
}

void CNLoginServer::killConnection(CNSocket* cns) {
    loginSessions.erase(cns);
}

void CNLoginServer::onStep() {
    time_t currTime = getTime();
    static time_t lastCheck = 0;

    if (currTime - lastCheck < 16000)
        return;
    lastCheck = currTime;

    for (auto& pair : loginSessions) {
        if (pair.second.lastHeartbeat != 0 && currTime - pair.second.lastHeartbeat > 32000) {
            pair.first->kill();
            continue;
        }

        INITSTRUCT(sP_LS2CL_REQ_LIVE_CHECK, pkt);
        pair.first->sendPacket((void*)&pkt, P_LS2CL_REQ_LIVE_CHECK, sizeof(sP_LS2CL_REQ_LIVE_CHECK));
    }
}

#pragma endregion

#pragma region helperMethods
bool CNLoginServer::isAccountInUse(int accountId) {
    std::map<CNSocket*, CNLoginData>::iterator it;
    for (it = CNLoginServer::loginSessions.begin(); it != CNLoginServer::loginSessions.end(); it++) {
        if (it->second.userID == accountId)
            return true;
    }
    return false;
}

bool CNLoginServer::exitDuplicate(int accountId) {
    std::map<CNSocket*, CNLoginData>::iterator it;
    for (it = CNLoginServer::loginSessions.begin(); it != CNLoginServer::loginSessions.end(); it++) {
        if (it->second.userID == accountId) {
            CNSocket* sock = it->first;
            INITSTRUCT(sP_LS2CL_REP_PC_EXIT_DUPLICATE, resp);
            resp.iErrorCode = 0;
            sock->sendPacket((void*)&resp, P_LS2CL_REP_PC_EXIT_DUPLICATE, sizeof(sP_LS2CL_REP_PC_EXIT_DUPLICATE));
            sock->kill();
            return true;
        }
    }
    return false;
}

bool CNLoginServer::isLoginDataGood(std::string login, std::string password) {
    std::regex loginRegex("[a-zA-Z0-9_-]{4,32}");
    std::regex passwordRegex("[a-zA-Z0-9!@#$%^&*()_+]{8,32}");

    return (std::regex_match(login, loginRegex) && std::regex_match(password, passwordRegex));
}

bool CNLoginServer::isPasswordCorrect(std::string actualPassword, std::string tryPassword) {
    return BCrypt::validatePassword(tryPassword, actualPassword);
}

bool CNLoginServer::isCharacterNameGood(std::string Firstname, std::string Lastname) {
    //Allow alphanumeric and dot characters in names(disallows dot and space characters at the beginning of a name)
    std::regex firstnamecheck(R"(((?! )(?!\.)[a-zA-Z0-9]*\.{0,1}(?!\.+ +)[a-zA-Z0-9]* {0,1}(?! +))*$)");
    std::regex lastnamecheck(R"(((?! )(?!\.)[a-zA-Z0-9]*\.{0,1}(?!\.+ +)[a-zA-Z0-9]* {0,1}(?! +))*$)");
    return (std::regex_match(Firstname, firstnamecheck) && std::regex_match(Lastname, lastnamecheck));
}
#pragma endregion
