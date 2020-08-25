#include "CNLoginServer.hpp"
#include "CNShared.hpp"
#include "CNStructs.hpp"

#include "settings.hpp"

/*
    This is *not* connected to any database, so i'm sending dummy data just to get the client to work & connect to the shard <3
*/

std::map<CNSocket*, CNLoginData> CNLoginServer::loginSessions;

CNLoginServer::CNLoginServer(uint16_t p) {
    port = p;
    pHandler = &CNLoginServer::handlePacket;
    init();
}

void CNLoginServer::handlePacket(CNSocket* sock, CNPacketData* data) {
    if (settings::VERBOSE)
        std::cout << "OpenFusion: received " << Defines::p2str(CL2LS, data->type) << " (" << data->type << ")" << std::endl;

    switch (data->type) {
        case P_CL2LS_REQ_LOGIN: {
            if (data->size != sizeof(sP_CL2LS_REQ_LOGIN))
                return; // ignore the malformed packet

            sP_CL2LS_REQ_LOGIN* login = (sP_CL2LS_REQ_LOGIN*)data->buf;
            INITSTRUCT(sP_LS2CL_REP_LOGIN_SUCC, resp);
            uint64_t cachedKey = sock->getEKey(); // so we can still send the resp packet with the correct key
            int charCount = 2; // send 4 randomly generated characters for now

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_LOGIN:" << std::endl;
                std::cout << "\tClient version: " << login->iClientVerA << "." << login->iClientVerB << "." << login->iClientVerC << std::endl;
                std::cout << "\tLogin type: " << login->iLoginType << std::endl;
                std::cout << "\tID: " << U16toU8(login->szID) << " Password: " << U16toU8(login->szPassword) << std::endl;
            )

            resp.iCharCount = charCount;
            resp.iSlotNum = 1;
            resp.iPaymentFlag = 1;
            resp.iOpenBetaFlag = 0;
            resp.uiSvrTime = getTime();

            // set username in resp packet
            memcpy(resp.szID, login->szID, sizeof(char16_t) * 33);

            // send the resp in with original key
            sock->sendPacket((void*)&resp, P_LS2CL_REP_LOGIN_SUCC, sizeof(sP_LS2CL_REP_LOGIN_SUCC));

            // update keys
            sock->setEKey(CNSocketEncryption::createNewKey(resp.uiSvrTime, resp.iCharCount + 1, resp.iSlotNum + 1));
            sock->setFEKey(CNSocketEncryption::createNewKey((uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]), login->iClientVerC, 1));

            loginSessions[sock] = CNLoginData();

            if (settings::LOGINRANDCHARACTERS) {
                // now send the characters :)
                for (int i = 0; i < charCount; i++) {
                    sP_LS2CL_REP_CHAR_INFO charInfo = sP_LS2CL_REP_CHAR_INFO();
                    charInfo.iSlot = (int8_t)i + 1;
                    charInfo.iLevel = (int16_t)1;
                    charInfo.sPC_Style.iPC_UID = rand(); // unique identifier for the character
                    charInfo.sPC_Style.iNameCheck = 1;
                    charInfo.sPC_Style.iGender = (i%2)+1; // can be 1(boy) or 2(girl)
                    charInfo.sPC_Style.iFaceStyle = 1;
                    charInfo.sPC_Style.iHairStyle = 1;
                    charInfo.sPC_Style.iHairColor = (rand()%19) + 1; // 1 - 19
                    charInfo.sPC_Style.iSkinColor = (rand()%13) + 1; // 1 - 13
                    charInfo.sPC_Style.iEyeColor = (rand()%6) + 1; // 1 - 6
                    charInfo.sPC_Style.iHeight = (rand()%6); // 0 -5
                    charInfo.sPC_Style.iBody = (rand()%4); // 0 - 3
                    charInfo.sPC_Style.iClass = 0;
                    charInfo.sPC_Style2.iAppearanceFlag = 1;
                    charInfo.sPC_Style2.iPayzoneFlag = 1;
                    charInfo.sPC_Style2.iTutorialFlag = 1;

                    // past's town hall
                    charInfo.iX = settings::SPAWN_X;
                    charInfo.iY = settings::SPAWN_Y;
                    charInfo.iZ = settings::SPAWN_Z;

                    U8toU16(std::string("Player"), charInfo.sPC_Style.szFirstName);
                    U8toU16(std::to_string(i + 1), charInfo.sPC_Style.szLastName);

                    int64_t UID = charInfo.sPC_Style.iPC_UID;
                    loginSessions[sock].characters[UID] = Player();
                    loginSessions[sock].characters[UID].level = charInfo.iLevel;
                    loginSessions[sock].characters[UID].money = 9001;
                    loginSessions[sock].characters[UID].slot = charInfo.iSlot;
                    loginSessions[sock].characters[UID].FEKey = sock->getFEKey();
                    loginSessions[sock].characters[UID].x = charInfo.iX;
                    loginSessions[sock].characters[UID].y = charInfo.iY;
                    loginSessions[sock].characters[UID].z = charInfo.iZ;
                    loginSessions[sock].characters[UID].PCStyle = charInfo.sPC_Style;
                    loginSessions[sock].characters[UID].PCStyle2 = charInfo.sPC_Style2;
                    loginSessions[sock].characters[UID].IsGM = false;

                    for (int i = 0; i < AEQUIP_COUNT; i++) {
                        // setup equips
                        charInfo.aEquip[i].iID = 0;
                        charInfo.aEquip[i].iType = i;
                        charInfo.aEquip[i].iOpt = 0;
                        loginSessions[sock].characters[UID].Equip[i] = charInfo.aEquip[i];
                    }
                    
                    for (int i = 0; i < AINVEN_COUNT; i++) {
                        // setup inventories
                        loginSessions[sock].characters[UID].Inven[i].iID = 0;
                        loginSessions[sock].characters[UID].Inven[i].iType = 0;
                        loginSessions[sock].characters[UID].Inven[i].iOpt = 0;
                    }
                    
                    // set default to the first character
                    if (i == 0)
                        loginSessions[sock].selectedChar = UID;

                    sock->sendPacket((void*)&charInfo, P_LS2CL_REP_CHAR_INFO, sizeof(sP_LS2CL_REP_CHAR_INFO));
                }
            }

            break;
        }
        case P_CL2LS_REP_LIVE_CHECK: {
            // stubbed, the client really doesn't care LOL
            break;
        }
        case P_CL2LS_REQ_CHECK_CHAR_NAME: {
            if (data->size != sizeof(sP_CL2LS_REQ_CHECK_CHAR_NAME))
                return;
            
            // naughty words allowed!!!!!!!! (also for some reason, the client will always show 'Player 0' if you manually type a name. It will show up for other connected players though)
            sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck = (sP_CL2LS_REQ_CHECK_CHAR_NAME*)data->buf;
            INITSTRUCT(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC, resp);

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_CHECK_CHAR_NAME:" << std::endl;
                std::cout << "\tFirstName: " << U16toU8(nameCheck->szFirstName) << " LastName: " << U16toU8(nameCheck->szLastName) << std::endl;
            )

            memcpy(resp.szFirstName, nameCheck->szFirstName, sizeof(char16_t) * 9);
            memcpy(resp.szLastName, nameCheck->szLastName, sizeof(char16_t) * 17);

            // fr*ck allowed!!!
            sock->sendPacket((void*)&resp, P_LS2CL_REP_CHECK_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC));
            break;
        }
        case P_CL2LS_REQ_SAVE_CHAR_NAME: {
            if (data->size != sizeof(sP_CL2LS_REQ_SAVE_CHAR_NAME))
                return;
            
            sP_CL2LS_REQ_SAVE_CHAR_NAME* save = (sP_CL2LS_REQ_SAVE_CHAR_NAME*)data->buf;
            INITSTRUCT(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC, resp);

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_SAVE_CHAR_NAME:" << std::endl;
                std::cout << "\tSlot: " << (int)save->iSlotNum << std::endl;
                std::cout << "\tGender: " << (int)save->iGender << std::endl;
                std::cout << "\tName: " << U16toU8(save->szFirstName) << " " << U16toU8(save->szLastName) << std::endl;
            )

            resp.iSlotNum = save->iSlotNum;
            resp.iGender = save->iGender;
            memcpy(resp.szFirstName, save->szFirstName, sizeof(char16_t) * 9);
            memcpy(resp.szLastName, save->szLastName, sizeof(char16_t) * 17);

            sock->sendPacket((void*)&resp, P_LS2CL_REP_SAVE_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC));
            break;
        }
        case P_CL2LS_REQ_CHAR_CREATE: {
            if (data->size != sizeof(sP_CL2LS_REQ_CHAR_CREATE))
                return;
            
            sP_CL2LS_REQ_CHAR_CREATE* character = (sP_CL2LS_REQ_CHAR_CREATE*)data->buf;
            INITSTRUCT(sP_LS2CL_REP_CHAR_CREATE_SUCC, resp);

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
            
            int64_t UID = character->PCStyle.iPC_UID;
            
            // commented and disabled for now
            //bool BecomeGM;
            
            //if (U16toU8(character->PCStyle.szFirstName) == settings::GMPASS) {
            //    BecomeGM = true;
            //    U8toU16("GM",character->PCStyle.szFirstName);
            //} else {
            //    BecomeGM = false;
            //}

            character->PCStyle.iNameCheck = 1;
            resp.sPC_Style = character->PCStyle;
            resp.sPC_Style2.iAppearanceFlag = 1;
            resp.sPC_Style2.iTutorialFlag = 1;
            resp.sPC_Style2.iPayzoneFlag = 1;
            resp.iLevel = 36;
            resp.sOn_Item = character->sOn_Item;

            loginSessions[sock].characters[UID] = Player();
            loginSessions[sock].characters[UID].level = 36;
            loginSessions[sock].characters[UID].money = 9001;
            loginSessions[sock].characters[UID].FEKey = sock->getFEKey();
            loginSessions[sock].characters[UID].PCStyle = character->PCStyle;
            loginSessions[sock].characters[UID].PCStyle2.iAppearanceFlag = 1;
            loginSessions[sock].characters[UID].PCStyle2.iPayzoneFlag = 1;
            loginSessions[sock].characters[UID].PCStyle2.iTutorialFlag = 1;
            loginSessions[sock].characters[UID].x = settings::SPAWN_X;
            loginSessions[sock].characters[UID].y = settings::SPAWN_Y;
            loginSessions[sock].characters[UID].z = settings::SPAWN_Z;
            loginSessions[sock].characters[UID].Equip[1].iID = character->sOn_Item.iEquipUBID; // upper body
            loginSessions[sock].characters[UID].Equip[1].iType = 1;
            loginSessions[sock].characters[UID].Equip[2].iID = character->sOn_Item.iEquipLBID; // lower body
            loginSessions[sock].characters[UID].Equip[2].iType = 2;
            loginSessions[sock].characters[UID].Equip[3].iID = character->sOn_Item.iEquipFootID; // foot!
            loginSessions[sock].characters[UID].Equip[3].iType = 3; 
            loginSessions[sock].characters[UID].IsGM = false;

            sock->sendPacket((void*)&resp, P_LS2CL_REP_CHAR_CREATE_SUCC, sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC));
            break;
        }
        case P_CL2LS_REQ_CHAR_SELECT: {
            if (data->size != sizeof(sP_CL2LS_REQ_CHAR_SELECT))
                return;
            
            // character selected
            sP_CL2LS_REQ_CHAR_SELECT* chararacter = (sP_CL2LS_REQ_CHAR_SELECT*)data->buf;
            INITSTRUCT(sP_LS2CL_REP_CHAR_SELECT_SUCC, resp);

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_CHAR_SELECT:" << std::endl;
                std::cout << "\tPC_UID: " << chararacter->iPC_UID << std::endl;
            )

            loginSessions[sock].selectedChar = chararacter->iPC_UID;

            sock->sendPacket((void*)&resp, P_LS2CL_REP_CHAR_SELECT_SUCC, sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC));
            break;
        }
        case P_CL2LS_REQ_SHARD_SELECT: {
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
            sock->kill(); // client should connect to the Shard server now
            break;
        }
        default:
            std::cerr << "OpenFusion: LOGIN UNIMPLM ERR. PacketType: " << Defines::p2str(CL2LS, data->type) << " (" << data->type << ")" << std::endl;
            break;
    }
}

void CNLoginServer::newConnection(CNSocket* cns) {
    cns->setActiveKey(SOCKETKEY_E); // by default they accept keys encrypted with the default key
}

void CNLoginServer::killConnection(CNSocket* cns) {
    loginSessions.erase(cns);
}
