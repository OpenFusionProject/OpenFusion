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
    switch (data->type) {
        case P_CL2LS_REQ_LOGIN: {
            if (data->size != sizeof(sP_CL2LS_REQ_LOGIN))
                return; // ignore the malformed packet

            sP_CL2LS_REQ_LOGIN* login = (sP_CL2LS_REQ_LOGIN*)data->buf;
            sP_LS2CL_REP_LOGIN_SUCC* response = (sP_LS2CL_REP_LOGIN_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_LOGIN_SUCC));
            uint64_t cachedKey = sock->getEKey(); // so we can still send the response packet with the correct key
            int charCount = 2; // send 4 randomly generated characters for now

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_LOGIN:" << std::endl;
                std::cout << "\tClient version: " << login->iClientVerA << "." << login->iClientVerB << "." << login->iClientVerC << std::endl;
                std::cout << "\tLogin type: " << login->iLoginType << std::endl;
                std::cout << "\tID: " << U16toU8(login->szID) << " Password: " << U16toU8(login->szPassword) << std::endl;
            )

            response->iCharCount = charCount;
            response->iSlotNum = 1;
            response->iPaymentFlag = 1;
            response->iOpenBetaFlag = 0;
            response->uiSvrTime = getTime();

            // set username in response packet
            memcpy(response->szID, login->szID, sizeof(char16_t) * 33);

            // update keys
            sock->setEKey(CNSocketEncryption::createNewKey(response->uiSvrTime, response->iCharCount + 1, response->iSlotNum + 1));
            sock->setFEKey(CNSocketEncryption::createNewKey((uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]), login->iClientVerC, 1));

             // send the response in with original key
            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_LOGIN_SUCC, sizeof(sP_LS2CL_REP_LOGIN_SUCC), cachedKey));

            loginSessions[sock] = CNLoginData();

            if (settings::LOGINRANDCHARACTERS) {
                // now send the characters :)
                for (int i = 0; i < charCount; i++) {
                    sP_LS2CL_REP_CHAR_INFO* charInfo = (sP_LS2CL_REP_CHAR_INFO*)xmalloc(sizeof(sP_LS2CL_REP_CHAR_INFO));
                    charInfo->iSlot = (int8_t)i + 1;
                    charInfo->iLevel = (int16_t)1;
                    charInfo->sPC_Style.iPC_UID = rand(); // unique identifier for the character
                    charInfo->sPC_Style.iNameCheck = 1;
                    charInfo->sPC_Style.iGender = (i%2)+1; // can be 1(boy) or 2(girl)
                    charInfo->sPC_Style.iFaceStyle = 1;
                    charInfo->sPC_Style.iHairStyle = 1;
                    charInfo->sPC_Style.iHairColor = (rand()%19) + 1; // 1 - 19
                    charInfo->sPC_Style.iSkinColor = (rand()%13) + 1; // 1 - 13
                    charInfo->sPC_Style.iEyeColor = (rand()%6) + 1; // 1 - 6
                    charInfo->sPC_Style.iHeight = (rand()%6); // 0 -5
                    charInfo->sPC_Style.iBody = (rand()%4); // 0 - 3
                    charInfo->sPC_Style.iClass = 0;
                    charInfo->sPC_Style2.iAppearanceFlag = 1;
                    charInfo->sPC_Style2.iPayzoneFlag = 1;
                    charInfo->sPC_Style2.iTutorialFlag = 1;

                    // past's town hall
                    charInfo->iX = settings::SPAWN_X;
                    charInfo->iY = settings::SPAWN_Y;
                    charInfo->iZ = settings::SPAWN_Z;

                    U8toU16(std::string("Player"), charInfo->sPC_Style.szFirstName);
                    U8toU16(std::to_string(i + 1), charInfo->sPC_Style.szLastName);

                    int64_t UID = charInfo->sPC_Style.iPC_UID;
                    loginSessions[sock].characters[UID] = Player();
                    loginSessions[sock].characters[UID].level = charInfo->iLevel;
                    loginSessions[sock].characters[UID].slot = charInfo->iSlot;
                    loginSessions[sock].characters[UID].FEKey = sock->getFEKey();
                    loginSessions[sock].characters[UID].x = charInfo->iX;
                    loginSessions[sock].characters[UID].y = charInfo->iY;
                    loginSessions[sock].characters[UID].z = charInfo->iZ;
                    loginSessions[sock].characters[UID].PCStyle = charInfo->sPC_Style;
                    loginSessions[sock].characters[UID].PCStyle2 = charInfo->sPC_Style2;

                    for (int i = 0; i < AEQUIP_COUNT; i++) {
                        // setup item
                        charInfo->aEquip[i].iID = 0;
                        charInfo->aEquip[i].iType = i;
                        charInfo->aEquip[i].iOpt = 0;
                        loginSessions[sock].characters[UID].Equip[i] = charInfo->aEquip[i];
                    }
                    
                    // set default to the first character
                    if (i == 0)
                        loginSessions[sock].selectedChar = UID;

                    sock->sendPacket(new CNPacketData((void*)charInfo, P_LS2CL_REP_CHAR_INFO, sizeof(sP_LS2CL_REP_CHAR_INFO), sock->getEKey()));
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
            sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC* response = (sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC));

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_CHECK_CHAR_NAME:" << std::endl;
                std::cout << "\tFirstName: " << U16toU8(nameCheck->szFirstName) << " LastName: " << U16toU8(nameCheck->szLastName) << std::endl;
            )

            memcpy(response->szFirstName, nameCheck->szFirstName, sizeof(char16_t) * 9);
            memcpy(response->szLastName, nameCheck->szLastName, sizeof(char16_t) * 17);

            // fr*ck allowed!!!
            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_CHECK_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC), sock->getEKey()));
            break;
        }
        case P_CL2LS_REQ_SAVE_CHAR_NAME: {
            if (data->size != sizeof(sP_CL2LS_REQ_SAVE_CHAR_NAME))
                return;
            
            sP_CL2LS_REQ_SAVE_CHAR_NAME* save = (sP_CL2LS_REQ_SAVE_CHAR_NAME*)data->buf;
            sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC* response = (sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC));

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_SAVE_CHAR_NAME:" << std::endl;
                std::cout << "\tSlot: " << (int)save->iSlotNum << std::endl;
                std::cout << "\tGender: " << (int)save->iGender << std::endl;
                std::cout << "\tName: " << U16toU8(save->szFirstName) << " " << U16toU8(save->szLastName) << std::endl;
            )

            response->iSlotNum = save->iSlotNum;
            response->iGender = save->iGender;
            memcpy(response->szFirstName, save->szFirstName, sizeof(char16_t) * 9);
            memcpy(response->szLastName, save->szLastName, sizeof(char16_t) * 17);

            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_SAVE_CHAR_NAME_SUCC, sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC), sock->getEKey()));
            break;
        }
        case P_CL2LS_REQ_CHAR_CREATE: {
            if (data->size != sizeof(sP_CL2LS_REQ_CHAR_CREATE))
                return;
            
            sP_CL2LS_REQ_CHAR_CREATE* character = (sP_CL2LS_REQ_CHAR_CREATE*)data->buf;
            sP_LS2CL_REP_CHAR_CREATE_SUCC* response = (sP_LS2CL_REP_CHAR_CREATE_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC));

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

            character->PCStyle.iNameCheck = 1;
            response->sPC_Style = character->PCStyle;
            response->sPC_Style2.iAppearanceFlag = 1;
            response->sPC_Style2.iTutorialFlag = 1;
            response->sPC_Style2.iPayzoneFlag = 1;
            response->iLevel = 1;
            response->sOn_Item = character->sOn_Item;

            int64_t UID = character->PCStyle.iPC_UID;
            loginSessions[sock].characters[UID] = Player();
            loginSessions[sock].characters[UID].level = 1;
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

            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_CHAR_CREATE_SUCC, sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC), sock->getEKey()));
            break;
        }
        case P_CL2LS_REQ_CHAR_SELECT: {
            if (data->size != sizeof(sP_CL2LS_REQ_CHAR_SELECT))
                return;
            
            // character selected
            sP_CL2LS_REQ_CHAR_SELECT* chararacter = (sP_CL2LS_REQ_CHAR_SELECT*)data->buf;
            sP_LS2CL_REP_CHAR_SELECT_SUCC* response = (sP_LS2CL_REP_CHAR_SELECT_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC));

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_CHAR_SELECT:" << std::endl;
                std::cout << "\tPC_UID: " << chararacter->iPC_UID << std::endl;
            )

            loginSessions[sock].selectedChar = chararacter->iPC_UID;

            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_CHAR_SELECT_SUCC, sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC), sock->getEKey()));
            break;
        }
        case P_CL2LS_REQ_SHARD_SELECT: {
            if (data->size != sizeof(sP_CL2LS_REQ_SHARD_SELECT))
                return;
            
            // tell client to connect to the shard server
            sP_CL2LS_REQ_SHARD_SELECT* shard = (sP_CL2LS_REQ_SHARD_SELECT*)data->buf;
            sP_LS2CL_REP_SHARD_SELECT_SUCC* response = (sP_LS2CL_REP_SHARD_SELECT_SUCC*)xmalloc(sizeof(sP_LS2CL_REP_SHARD_SELECT_SUCC));

            DEBUGLOG(
                std::cout << "P_CL2LS_REQ_SHARD_SELECT:" << std::endl;
                std::cout << "\tShard: " << (int)shard->ShardNum << std::endl;
            )

            const char* SHARD_IP = settings::SHARDSERVERIP.c_str();
            response->iEnterSerialKey = rand();
            response->g_FE_ServerPort = settings::SHARDPORT;

            // copy IP to response (this struct uses ASCII encoding so we don't have to goof around converting encodings)
            memcpy(response->g_FE_ServerIP, SHARD_IP, strlen(SHARD_IP));
            response->g_FE_ServerIP[strlen(SHARD_IP)] = '\0';

            // pass player to CNSharedData
            CNSharedData::setPlayer(response->iEnterSerialKey, loginSessions[sock].characters[loginSessions[sock].selectedChar]);

            sock->sendPacket(new CNPacketData((void*)response, P_LS2CL_REP_SHARD_SELECT_SUCC, sizeof(sP_LS2CL_REP_SHARD_SELECT_SUCC), sock->getEKey()));
            sock->kill(); // client should connect to the Shard server now
            break;
        }
        default:
            std::cerr << "OpenFusion: LOGIN UNIMPLM ERR. PacketType: " << data->type << std::endl;
            break;
    }
}

void CNLoginServer::killConnection(CNSocket* cns) {
    loginSessions.erase(cns);
}