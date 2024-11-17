#include "servers/CNLoginServer.hpp"

#include "core/CNShared.hpp"
#include "db/Database.hpp"
#include "bcrypt/BCrypt.hpp"

#include "PlayerManager.hpp"
#include "Items.hpp"
#include "settings.hpp"

#include <regex>

std::map<CNSocket*, CNLoginData> CNLoginServer::loginSessions;

CNLoginServer::CNLoginServer(uint16_t p) {
    serverType = "login";
    port = p;
    pHandler = &CNLoginServer::handlePacket;
    init();
}

void CNLoginServer::handlePacket(CNSocket* sock, CNPacketData* data) {
    printPacket(data);

    if (loginSessions.find(sock) == loginSessions.end() &&
        data->type != P_CL2LS_REQ_LOGIN && data->type != P_CL2LS_REP_LIVE_CHECK) {

        if (settings::VERBOSITY > 0) {
            std::cerr << "OpenFusion: LOGIN PKT OUT-OF-SEQ. PacketType: " <<
                Packets::p2str(data->type) << " (" << data->type << ")" << std::endl;
        }

        return;
    }

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
                std::cerr << "OpenFusion: LOGIN UNIMPLM ERR. PacketType: " << Packets::p2str(data->type) << " (" << data->type << ")" << std::endl;
            break;
        /*
         * Unimplemented CL2LS packets:
         *  P_CL2LS_REQ_SHARD_SELECT - we skip it in char select
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
    sock->sendPacket(resp, P_LS2CL_REP_LOGIN_FAIL);

    DEBUGLOG(
        std::cout << "Login Server: Login fail. Error code " << (int)errorCode << std::endl;
    )

    return;
}

void CNLoginServer::login(CNSocket* sock, CNPacketData* data) {
    auto login = (sP_CL2LS_REQ_LOGIN*)data->buf;

    std::string userLogin;
    std::string userToken; // could be password or auth cookie

    /*
     * In this context, "cookie auth" just means the credentials were sent
     * in the szCookie fields instead of szID and szPassword.
     */
    bool isCookieAuth = login->iLoginType == USE_COOKIE_FIELDS;

    /*
     * The std::string -> char* -> std::string maneuver should remove any
     * trailing garbage after the null terminator.
     */
    if (isCookieAuth) {
        userLogin = std::string(AUTOU8(login->szCookie_TEGid).c_str());
        userToken = std::string(AUTOU8(login->szCookie_authid).c_str());
    } else {
        userLogin = std::string(AUTOU16TOU8(login->szID).c_str());
        userToken = std::string(AUTOU16TOU8(login->szPassword).c_str());
    }

    if (!CNLoginServer::checkUsername(sock, userLogin)) {
        return loginFail(LoginError::LOGIN_ERROR, userLogin, sock);
    }

    if (!isCookieAuth) {
        // password was sent in plaintext
        if (!CNLoginServer::checkPassword(sock, userToken)) {
            return loginFail(LoginError::LOGIN_ERROR, userLogin, sock);
        }
    }

    Database::Account findUser = {};
    Database::findAccount(&findUser, userLogin);

    // account was not found
    if (findUser.AccountID == 0) {
        /*
         * Don't auto-create accounts if it's a cookie login.
         * It'll either be a bad cookie or a plaintext password sent by auto-login;
         * either way, we only want to allow auto-creation if the user explicitly entered their credentials.
         */
        if (settings::AUTOCREATEACCOUNTS && !isCookieAuth) {
            return newAccount(sock, userLogin, userToken, login->iClientVerC);
        }

        return loginFail(LoginError::ID_DOESNT_EXIST, userLogin, sock);
    }

    // make sure either a valid cookie or password was sent
    if (!CNLoginServer::checkToken(sock, findUser, userToken, isCookieAuth)) {
        return loginFail(LoginError::ID_AND_PASSWORD_DO_NOT_MATCH, userLogin, sock);
    }

    if (CNLoginServer::checkBan(sock, findUser)) {
        return; // don't send fail packet
    }

    /* 
     * calling this here to timestamp login attempt,
     * in order to make duplicate exit sanity check work
     */
    Database::updateSelected(findUser.AccountID, findUser.Selected);

    if (CNLoginServer::isAccountInUse(findUser.AccountID))
        return loginFail(LoginError::ID_ALREADY_IN_USE, userLogin, sock);

    loginSessions[sock] = CNLoginData();
    loginSessions[sock].userID = findUser.AccountID;
    loginSessions[sock].lastHeartbeat = getTime();

    std::vector<sP_LS2CL_REP_CHAR_INFO> characters;
    Database::getCharInfo(&characters, loginSessions[sock].userID);

    INITSTRUCT(sP_LS2CL_REP_LOGIN_SUCC, resp);
    memcpy(resp.szID, login->szID, sizeof(login->szID));

    resp.iCharCount = characters.size();
    resp.iSlotNum = findUser.Selected;
    resp.iPaymentFlag = 1;
    resp.iOpenBetaFlag = 0;
    resp.uiSvrTime = getTime();

#ifdef ACADEMY
    // do not prompt player to "enable freechat", which would only close the client
    resp.iChatEnabled = 1;
#endif

    // send the resp in with original key
    sock->sendPacket(resp, P_LS2CL_REP_LOGIN_SUCC);

    uint64_t defaultKey;
    memcpy(&defaultKey, CNSocketEncryption::defaultKey, sizeof(defaultKey));

    // update keys
    sock->setEKey(CNSocketEncryption::createNewKey(resp.uiSvrTime, resp.iCharCount + 1, resp.iSlotNum + 1));
    sock->setFEKey(CNSocketEncryption::createNewKey(defaultKey, login->iClientVerC, 1));

    DEBUGLOG(
        std::cout << "Login Server: Login success. Welcome " << userLogin << " [" << loginSessions[sock].userID << "]" << std::endl;
    )
        
    if (resp.iCharCount == 0)
        return;

    // now send the characters :)
    std::vector<sP_LS2CL_REP_CHAR_INFO>::iterator it;
    for (it = characters.begin(); it != characters.end(); it++)
        sock->sendPacket(*it, P_LS2CL_REP_CHAR_INFO);

    DEBUGLOG(
        std::string message = "Login Server: Loaded " + std::to_string(resp.iCharCount) + " character";
        if ((int)resp.iCharCount > 1)
            message += "s";
        std::cout << message << std::endl;
    )
}

void CNLoginServer::newAccount(CNSocket* sock, std::string userLogin, std::string userPassword, int32_t clientVerC) {   
    int userID = Database::addAccount(userLogin, userPassword);
    // if query somehow failed
    if (userID == 0)
        return loginFail(LoginError::DATABASE_ERROR, userLogin, sock);

    loginSessions[sock] = CNLoginData();
    loginSessions[sock].userID = userID;
    loginSessions[sock].lastHeartbeat = getTime();

    INITSTRUCT(sP_LS2CL_REP_LOGIN_SUCC, resp);
    U8toU16(userLogin, resp.szID, sizeof(resp.szID));

    resp.iCharCount = 0;
    resp.iSlotNum = 1;
    resp.iPaymentFlag = 1;
    resp.iOpenBetaFlag = 0;
    resp.uiSvrTime = getTime();

    // send the resp in with original key
    sock->sendPacket(resp, P_LS2CL_REP_LOGIN_SUCC);

    // update keys
    sock->setEKey(CNSocketEncryption::createNewKey(resp.uiSvrTime, resp.iCharCount + 1, resp.iSlotNum + 1));
    sock->setFEKey(CNSocketEncryption::createNewKey((uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]), clientVerC, 1));

    DEBUGLOG(
        std::cout << "Login Server: New account. Welcome " << userLogin << " [" << loginSessions[sock].userID << "]" << std::endl;
    )
}

void CNLoginServer::nameCheck(CNSocket* sock, CNPacketData* data) {
    // responding to this packet only makes the client send the next packet (either name save or name change)
    // so we're always sending SUCC here and actually validating the name when the next packet arrives

    auto nameCheck = (sP_CL2LS_REQ_CHECK_CHAR_NAME*)data->buf;

    INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC, resp);
    memcpy(resp.szFirstName, nameCheck->szFirstName, sizeof(resp.szFirstName));
    memcpy(resp.szLastName, nameCheck->szLastName, sizeof(resp.szLastName));
    sock->sendPacket(resp, P_LS2CL_REP_CHECK_CHAR_NAME_SUCC);

    loginSessions[sock].lastHeartbeat = getTime();
}

void invalidCharacter(CNSocket* sock) {
    INITSTRUCT(sP_LS2CL_REP_SHARD_SELECT_FAIL, fail);
    fail.iErrorCode = 2;
    sock->sendPacket(fail, P_LS2CL_REP_SHARD_SELECT_FAIL);

    DEBUGLOG(
        std::cout << "Login Server: Selected character error" << std::endl;
    )
        return;
}

void CNLoginServer::nameSave(CNSocket* sock, CNPacketData* data) {
    auto save = (sP_CL2LS_REQ_SAVE_CHAR_NAME*)data->buf;
    INITSTRUCT(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC, resp);

    int errorCode = 0;
    if (!CNLoginServer::isCharacterNameGood(AUTOU16TOU8(save->szFirstName), AUTOU16TOU8(save->szLastName))) {
        errorCode = 4;
    } else if (!Database::isNameFree(AUTOU16TOU8(save->szFirstName), AUTOU16TOU8(save->szLastName))) {
        errorCode = 1;
    }

    if (errorCode != 0) {
        INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL, resp);
        resp.iErrorCode = errorCode;
        sock->sendPacket(resp, P_LS2CL_REP_CHECK_CHAR_NAME_FAIL);

        DEBUGLOG(
            std::cout << "Login Server: name check fail. Error code " << errorCode << std::endl;
        )

        return;
    }

    if (!Database::isSlotFree(loginSessions[sock].userID, save->iSlotNum))
        return invalidCharacter(sock);

    resp.iPC_UID = Database::createCharacter(save, loginSessions[sock].userID);
    // if query somehow failed
    if (resp.iPC_UID == 0) {
        std::cout << "[WARN] Login Server: Database failed to create new character!" << std::endl;
        return invalidCharacter(sock);
    }
    resp.iSlotNum = save->iSlotNum;
    resp.iGender = save->iGender;
    
    memcpy(resp.szFirstName, save->szFirstName, sizeof(resp.szFirstName));
    memcpy(resp.szLastName, save->szLastName, sizeof(resp.szLastName));

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket(resp, P_LS2CL_REP_SAVE_CHAR_NAME_SUCC);

    Database::updateSelected(loginSessions[sock].userID, save->iSlotNum);

    DEBUGLOG(
        std::cout << "Login Server: new character created" << std::endl;
        std::cout << "\tSlot: " << (int)save->iSlotNum << std::endl;
        std::cout << "\tName: " << AUTOU16TOU8(save->szFirstName) << " " << AUTOU16TOU8(save->szLastName) << std::endl;
    )
}

bool validateCharacterCreation(sP_CL2LS_REQ_CHAR_CREATE* character) {
    // all the values have been determined from analyzing client code and xdt
    // and double checked using cheat engine

    // check base parameters
    sPCStyle* style = &character->PCStyle;
    if (!(style->iBody      >= 0 && style->iBody      <= 2   &&
          style->iEyeColor  >= 1 && style->iEyeColor  <= 5   &&
          style->iGender    >= 1 && style->iGender    <= 2   &&
          style->iHairColor >= 1 && style->iHairColor <= 18) &&
          style->iHeight    >= 0 && style->iHeight    <= 4   &&
          style->iNameCheck >= 0 && style->iNameCheck <= 2   &&
          style->iSkinColor >= 1 && style->iSkinColor <= 12)
        return false;
            
    // facestyle and hairstyle are gender dependent
    if (!(style->iGender == 1 && style->iFaceStyle >= 1 && style->iFaceStyle <= 5  && style->iHairStyle >= 1  && style->iHairStyle <= 23) &&
        !(style->iGender == 2 && style->iFaceStyle >= 6 && style->iFaceStyle <= 10 && style->iHairStyle >= 25 && style->iHairStyle <= 45))
        return false;

    // validate items
    std::pair<int32_t, int32_t> items[3];
    items[0] = std::make_pair(character->sOn_Item.iEquipUBID, 1);
    items[1] = std::make_pair(character->sOn_Item.iEquipLBID, 2);
    items[2] = std::make_pair(character->sOn_Item.iEquipFootID, 3);
    // once we have a static database perhaps we can check for the exact char creation items,
    // for now only checking if it's a valid lvl1 item
    for (int i = 0; i < 3; i++) {
        if (Items::ItemData.find(items[i]) == Items::ItemData.end() || Items::ItemData[items[i]].level != 1)
            return false;
    }
    return true;
}

void CNLoginServer::characterCreate(CNSocket* sock, CNPacketData* data) {
    auto character = (sP_CL2LS_REQ_CHAR_CREATE*)data->buf;

    if (!validateCharacterCreation(character))
    {
        std::cout << "[WARN] Login Server: invalid CHAR_CREATE packet!" << std::endl;
        return invalidCharacter(sock);
    }
    if (!Database::finishCharacter(character, loginSessions[sock].userID))
    {
        std::cout << "[WARN] Login Server: Database failed to finish character creation!" << std::endl;
        return invalidCharacter(sock);
    }
    
    Player player = {};
    Database::getPlayer(&player, character->PCStyle.iPC_UID);

    INITSTRUCT(sP_LS2CL_REP_CHAR_CREATE_SUCC, resp);
    resp.sPC_Style = player.PCStyle;
    resp.sPC_Style2 = player.PCStyle2;
    resp.iLevel = player.level;
    resp.sOn_Item.iEquipUBID = player.Equip[1].iID;
    resp.sOn_Item.iEquipLBID = player.Equip[2].iID;
    resp.sOn_Item.iEquipFootID = player.Equip[3].iID;

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket(resp, P_LS2CL_REP_CHAR_CREATE_SUCC);
    Database::updateSelected(loginSessions[sock].userID, player.slot);   

    DEBUGLOG(
        std::cout << "Login Server: Character creation completed" << std::endl;
        std::cout << "\tPC_UID: " << character->PCStyle.iPC_UID << std::endl;
        std::cout << "\tNameCheck: " << (int)character->PCStyle.iNameCheck << std::endl;
        std::cout << "\tName: " << AUTOU16TOU8(character->PCStyle.szFirstName) << " " << AUTOU16TOU8(character->PCStyle.szLastName) << std::endl;
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
}

void CNLoginServer::characterDelete(CNSocket* sock, CNPacketData* data) {
    auto del = (sP_CL2LS_REQ_CHAR_DELETE*)data->buf;

    int removedSlot = Database::deleteCharacter(del->iPC_UID, loginSessions[sock].userID);
    if (removedSlot == 0)
        return invalidCharacter(sock);

    INITSTRUCT(sP_LS2CL_REP_CHAR_DELETE_SUCC, resp);
    resp.iSlotNum = removedSlot;
    sock->sendPacket(resp, P_LS2CL_REP_CHAR_DELETE_SUCC);
    loginSessions[sock].lastHeartbeat = getTime();

    DEBUGLOG(
        std::cout << "Login Server: Character [" << del->iPC_UID << "] deleted" << std::endl;
    )
}

void CNLoginServer::characterSelect(CNSocket* sock, CNPacketData* data) {
    auto selection = (sP_CL2LS_REQ_CHAR_SELECT*)data->buf;
    // we're doing a small hack and immediately send SHARD_SELECT_SUCC
    INITSTRUCT(sP_LS2CL_REP_SHARD_SELECT_SUCC, resp);

    if (!Database::validateCharacter(selection->iPC_UID, loginSessions[sock].userID))
        return invalidCharacter(sock);

    DEBUGLOG(
        std::cout << "Login Server: Selected character [" << selection->iPC_UID << "]" << std::endl;
        std::cout << "Connecting to shard server" << std::endl;
    )

    const char* shard_ip = settings::SHARDSERVERIP.c_str();

    /*
     * Work around the issue of not being able to connect to a local server if
     * the shard IP has been configured to an address the local machine can't
     * reach itself from.
     */
    if (settings::LOCALHOSTWORKAROUND && sock->sockaddr.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
        shard_ip = "127.0.0.1";

    memcpy(resp.g_FE_ServerIP, shard_ip, strlen(shard_ip));

    resp.g_FE_ServerIP[strlen(shard_ip)] = '\0';
    resp.g_FE_ServerPort = settings::SHARDPORT;

    LoginMetadata *lm = new LoginMetadata();
    lm->FEKey = sock->getFEKey();
    lm->timestamp = getTime();
    lm->playerId = selection->iPC_UID;

    resp.iEnterSerialKey = Rand::cryptoRand();

    // transfer ownership of connection data to CNShared
    CNShared::storeLoginMetadata(resp.iEnterSerialKey, lm);

    sock->sendPacket(resp, P_LS2CL_REP_SHARD_SELECT_SUCC);

    // update current slot in DB
    Database::updateSelectedByPlayerId(loginSessions[sock].userID, selection->iPC_UID);
}

void CNLoginServer::finishTutorial(CNSocket* sock, CNPacketData* data) {
    auto save = (sP_CL2LS_REQ_SAVE_CHAR_TUTOR*)data->buf;

    if (!Database::finishTutorial(save->iPC_UID, loginSessions[sock].userID))
        return invalidCharacter(sock);

    loginSessions[sock].lastHeartbeat = getTime();
    // no response here

    DEBUGLOG(
        std::cout << "Login Server: Character [" << save->iPC_UID << "] completed tutorial" << std::endl;
    )
}

void CNLoginServer::changeName(CNSocket* sock, CNPacketData* data) {
    auto save = (sP_CL2LS_REQ_CHANGE_CHAR_NAME*)data->buf;

    int errorCode = 0;
    if (!CNLoginServer::isCharacterNameGood(AUTOU16TOU8(save->szFirstName), AUTOU16TOU8(save->szLastName))) {
        errorCode = 4;
    }
    else if (!Database::isNameFree(AUTOU16TOU8(save->szFirstName), AUTOU16TOU8(save->szLastName))) {
        errorCode = 1;
    }

    if (errorCode != 0) {
        INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL, resp);
        resp.iErrorCode = errorCode;
        sock->sendPacket(resp, P_LS2CL_REP_CHECK_CHAR_NAME_FAIL);

        DEBUGLOG(
            std::cout << "Login Server: name check fail. Error code " << errorCode << std::endl;
        )

        return;
    }

    if (!Database::changeName(save, loginSessions[sock].userID))
        return invalidCharacter(sock);

    INITSTRUCT(sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC, resp);
    resp.iPC_UID = save->iPCUID;
    memcpy(resp.szFirstName, save->szFirstName, sizeof(resp.szFirstName));
    memcpy(resp.szLastName, save->szLastName, sizeof(resp.szLastName));
    resp.iSlotNum = save->iSlotNum;

    loginSessions[sock].lastHeartbeat = getTime();

    sock->sendPacket(resp, P_LS2CL_REP_CHANGE_CHAR_NAME_SUCC);

    DEBUGLOG(
        std::cout << "Login Server: Name check success for character [" << save->iPCUID << "]" << std::endl;
        std::cout << "\tNew name: " << AUTOU16TOU8(save->szFirstName) << " " << AUTOU16TOU8(save->szLastName) << std::endl;     
    )
}

void CNLoginServer::duplicateExit(CNSocket* sock, CNPacketData* data) {
    // TODO: FIX THIS PACKET

    sP_CL2LS_REQ_PC_EXIT_DUPLICATE* exit = (sP_CL2LS_REQ_PC_EXIT_DUPLICATE*)data->buf;
    Database::Account account = {};
    Database::findAccount(&account, AUTOU16TOU8(exit->szID));

    // sanity check
    if (account.AccountID == 0) {
        std::cout << "[WARN] P_CL2LS_REQ_PC_EXIT_DUPLICATE submitted unknown username: " << exit->szID << std::endl;
        return;
    }

    exitDuplicate(account.AccountID);
}
#pragma endregion

#pragma region connections
void CNLoginServer::newConnection(CNSocket* cns) {
    cns->setActiveKey(SOCKETKEY_E); // by default they accept keys encrypted with the default key
}

void CNLoginServer::killConnection(CNSocket* cns) {
    DEBUGLOG(
        std::cout << "Login Server: Account [" << loginSessions[cns].userID << "] disconnected from login server" << std::endl;
    )
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
        pair.first->sendPacket(pkt, P_LS2CL_REQ_LIVE_CHECK);
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
            sock->sendPacket(resp, P_LS2CL_REP_PC_EXIT_DUPLICATE);
            sock->kill();
            return true;
        }
    }
    return false;
}

bool CNLoginServer::isUsernameGood(std::string& login) {
    const std::regex loginRegex("[a-zA-Z0-9_-]{4,32}");
    return (std::regex_match(login, loginRegex));
}

bool CNLoginServer::isPasswordGood(std::string& password) {
    const std::regex passwordRegex("[a-zA-Z0-9!@#$%^&*()_+]{8,32}");
    return (std::regex_match(password, passwordRegex));
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

bool CNLoginServer::isAuthMethodAllowed(AuthMethod authMethod) {
    // the config file specifies "comma-separated" but tbh we don't care
    switch (authMethod) {
    case AuthMethod::PASSWORD:
        return settings::AUTHMETHODS.find("password") != std::string::npos;
    case AuthMethod::COOKIE:
        return settings::AUTHMETHODS.find("cookie") != std::string::npos;
    default:
        break;
    }
    return false;
}

bool CNLoginServer::checkPassword(CNSocket* sock, std::string& password) {
    // check password auth allowed
    if (!CNLoginServer::isAuthMethodAllowed(AuthMethod::PASSWORD)) {
        // send a custom error message
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        std::string text = "Password login disabled\n";
        text += "This server has disabled logging in with plaintext passwords.\n";
        text += "Please contact an admin for assistance.";
        U8toU16(text, msg.szAnnounceMsg, sizeof(msg.szAnnounceMsg));
        msg.iDuringTime = 12;
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);

        return false;
    }

    // check regex
    if (!CNLoginServer::isPasswordGood(password)) {
        // send a custom error message
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        std::string text = "Invalid password\n";
        text += "Password has to be 8 - 32 characters long";
        U8toU16(text, msg.szAnnounceMsg, sizeof(msg.szAnnounceMsg));
        msg.iDuringTime = 10;
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);

        return false;
    }

    return true;
}

bool CNLoginServer::checkUsername(CNSocket* sock, std::string& username) {
    // check username regex
    if (!CNLoginServer::isUsernameGood(username)) {
        // send a custom error message
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        std::string text = "Invalid login\n";
        text += "Login has to be 4 - 32 characters long and can't contain special characters other than dash and underscore";
        U8toU16(text, msg.szAnnounceMsg, sizeof(msg.szAnnounceMsg));
        msg.iDuringTime = 10;
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);

        return false;
    }

    return true;
}

bool CNLoginServer::checkToken(CNSocket* sock, Database::Account& account, std::string& token, bool isCookieAuth) {
    // check for valid cookie first
    if (isCookieAuth && CNLoginServer::isAuthMethodAllowed(AuthMethod::COOKIE)) {
        const char *cookie = token.c_str();
        if (Database::checkCookie(account.AccountID, cookie)) {
            return true;
        }
    }

    // cookie check failed; check to see if it's a plaintext password
    if (CNLoginServer::isAuthMethodAllowed(AuthMethod::PASSWORD)
        && CNLoginServer::isPasswordCorrect(account.Password, token)) {
        return true;
    }

    return false;
}

bool CNLoginServer::checkBan(CNSocket* sock, Database::Account& account) {
    // check if the account is banned
    if (account.BannedUntil > getTimestamp()) {
        // send a custom error message
        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);

        // ceiling devision
        int64_t remainingDays = (account.BannedUntil-getTimestamp()) / 86400 + ((account.BannedUntil - getTimestamp()) % 86400 != 0);

        std::string text = "Your account has been banned. \nReason: ";
        text += account.BanReason;
        text += "\nBan expires in " + std::to_string(remainingDays) + " day";
        if (remainingDays > 1)
            text += "s";

        U8toU16(text, msg.szAnnounceMsg, sizeof(msg.szAnnounceMsg));
        msg.iDuringTime = 99999999;
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);

        return true;
    }

    return false;
}
#pragma endregion
