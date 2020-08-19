/* 
    CNStructs.hpp - defines some basic structs & useful methods for packets used by FusionFall

    NOTE: this is missing the vast majority of packets, I have also ommitted the ERR & FAIL packets for simplicity
*/

#ifndef _CNS_HPP
#define _CNS_HPP

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <cstring>
#include <string> 
#include <locale> 
#include <codecvt> 

// TODO: rewrite U16toU8 & U8toU16 to not use codecvt

std::string U16toU8(char16_t* src);

// returns number of char16_t that was written at des
int U8toU16(std::string src, char16_t* des);

uint64_t getTime();

//#define CNPROTO_VERSION_0728

#ifdef CNPROTO_VERSION_0728
	#define AEQUIP_COUNT 12
#else
	#define AEQUIP_COUNT 9
#endif

// ========================================================[[ General Purpose ]]========================================================

// sets the same byte alignment as the structs in the client
#pragma pack(push, 4)
struct sPCStyle {
	int64_t iPC_UID;
	int8_t iNameCheck;

	char16_t szFirstName[9];
	char16_t szLastName[17];

	int8_t iGender;
	int8_t iFaceStyle;
	int8_t iHairStyle;
    int8_t iHairColor;
    int8_t iSkinColor;
    int8_t iEyeColor;
    int8_t iHeight;
    int8_t iBody;
    int32_t iClass;
};

#pragma pack(1)
struct sPCStyle2 {
	int8_t iAppearanceFlag;
	int8_t iTutorialFlag;
	int8_t iPayzoneFlag;

    sPCStyle2() {}
    sPCStyle2(int8_t a, int8_t t, int8_t p):
        iAppearanceFlag(a), iTutorialFlag(t), iPayzoneFlag(p) {}
};

#pragma pack(2)
struct sOnItem {
	int16_t iEquipHandID;
	int16_t iEquipUBID;
	int16_t iEquipLBID;
	int16_t iEquipFootID;
	int16_t iEquipHeadID;
	int16_t iEquipFaceID;
	int16_t iEquipBackID;
};

struct sOnItem_Index {
	int16_t iEquipUBID_index;
	int16_t iEquipLBID_index;
	int16_t iEquipFootID_index;
	int16_t iFaceStyle;
	int16_t iHairStyle;
};

struct sNano {
	int16_t iID;
	int16_t iSkillID;
	int16_t iStamina;
};

#pragma pack(4)
struct sItemBase {
	int16_t iType;
	int16_t iID;
	int32_t iOpt;
	int32_t iTimeLimit;

#ifdef CNPROTO_VERSION_0728
	int32_t iSerial;
#endif
};

struct sTimeBuff {
	uint64_t iTimeLimit;
	uint64_t iTimeDuration;

	int32_t iTimeRepeat;
	int32_t iValue;
	int32_t iConfirmNum;
};

struct sRunningQuest {
	int32_t m_aCurrTaskID;

	int32_t m_aKillNPCID[3];
    int32_t m_aKillNPCCount[3];
    int32_t m_aNeededItemID[3];
	int32_t m_aNeededItemCount[3];
};

struct sPCLoadData2CL {
	int16_t iUserLevel;
    sPCStyle PCStyle;
    sPCStyle2 PCStyle2;

	int16_t iLevel;
	int16_t iMentor;
	int16_t iMentorCount;
	int32_t iHP;
	int32_t iBatteryW;
	int32_t iBatteryN;
	int32_t iCandy;
	int32_t iFusionMatter;
	int8_t iSpecialState;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;

	sItemBase aEquip[AEQUIP_COUNT];
	sItemBase aInven[50];
    sItemBase aQInven[50];

	sNano aNanoBank[37];

	int16_t aNanoSlots[3];

	int16_t iActiveNanoSlotNum;
	int32_t iConditionBitFlag;
	int32_t eCSTB___Add;

	sTimeBuff TimeBuff;

    int64_t aQuestFlag[32];
	int64_t aRepeatQuestFlag[8];

	sRunningQuest aRunningQuest[9];

	int32_t iCurrentMissionID;
	int32_t iWarpLocationFlag;

	int64_t aWyvernLocationFlag[2];

	int32_t iBuddyWarpTime;
	int32_t iFatigue;
	int32_t iFatigue_Level;
	int32_t iFatigueRate;
	int64_t iFirstUseFlag1;
	int64_t iFirstUseFlag2;

	int32_t aiPCSkill[33];

    sPCLoadData2CL() {};
};

struct sPCAppearanceData {
	int32_t iID;
	sPCStyle PCStyle;

	int32_t iConditionBitFlag;
	int8_t iPCState;
	int8_t iSpecialState;
	int16_t iLv;
	int32_t iHP;

	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;

	sItemBase ItemEquip[AEQUIP_COUNT];
	sNano Nano;

	int32_t eRT;
};

// ========================================================[[ Client2LoginServer packets ]]========================================================

#pragma pack(4)
struct sP_CL2LS_REQ_LOGIN {
    char16_t szID[33];
    char16_t szPassword[33];

    int32_t iClientVerA;
    int32_t iClientVerB;
    int32_t iClientVerC;

    int32_t iLoginType;

    uint8_t szCookie_TEGid[64];
    uint8_t szCookie_authid[255];
};

struct sP_CL2LS_REQ_CHECK_CHAR_NAME {
    int32_t iFNCode;
    int32_t iLNCode;
    int32_t iMNCode;

    char16_t szFirstName[9];
    char16_t szLastName[17];
};

struct sP_CL2LS_REQ_SAVE_CHAR_NAME {
    int8_t iSlotNum;
    int8_t iGender;
    int32_t iFNCode;
    int32_t iLNCode;
    int32_t iMNCode;

    char16_t szFirstName[9];
    char16_t szLastName[17];
};

struct sP_CL2LS_REQ_CHAR_CREATE {
	sPCStyle PCStyle;
    sOnItem sOn_Item;
    sOnItem_Index sOn_Item_Index;
};

struct sP_CL2LS_REQ_CHAR_SELECT {
	int64_t iPC_UID;
};

struct sP_CL2LS_REP_LIVE_CHECK {
    int32_t unused;
};

#pragma pack(1)
struct sP_CL2LS_REQ_SHARD_SELECT {
    int8_t iShardNum;
};

struct sP_CL2LS_REQ_SHARD_LIST_INFO {
    uint8_t unused;
};

// ========================================================[[ LoginServer2Client packets ]]========================================================

#pragma pack(4)
struct sP_LS2CL_REP_LOGIN_SUCC {
    int8_t iCharCount;
    int8_t iSlotNum;
    int8_t iPaymentFlag;
    int8_t iTempForPacking4; // UNUSED
    uint64_t uiSvrTime; // UNIX timestamp

    char16_t szID[33];

    uint32_t iOpenBetaFlag;
};

#pragma pack(2)
struct sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC {
    char16_t szFirstName[9];
    char16_t szLastName[17];
};

#pragma pack(4)
struct sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC {
    int64_t iPC_UID;
    int8_t iSlotNum;
    int8_t iGender;

    char16_t szFirstName[9];
    char16_t szLastName[17];
};

struct sP_LS2CL_REP_CHAR_CREATE_SUCC {
	int16_t iLevel;
    sPCStyle PC_Style;
    sPCStyle2 PC_Style2;
    sOnItem sOn_Item;
};

struct sP_LS2CL_REP_CHAR_INFO {
	int8_t iSlot;
	int16_t iLevel;

	sPCStyle sPC_Style;
	sPCStyle2 sPC_Style2;

	int32_t iX;
	int32_t iY;
	int32_t iZ;

    sItemBase aEquip[AEQUIP_COUNT];
};

struct sP_LS2CL_REP_SHARD_SELECT_SUCC {
	uint8_t g_FE_ServerIP[16]; // Ascii
	int32_t g_FE_ServerPort;
	int64_t iEnterSerialKey;
};

#pragma pack(1)
struct sP_LS2CL_REP_CHAR_SELECT_SUCC {
    int8_t unused;
};

struct sP_LS2CL_REP_SHARD_LIST_INFO_SUCC {
    uint8_t aShardConnectFlag[27];
};


// ========================================================[[ Client2ShardServer packets ]]========================================================

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ENTER {
	char16_t szID[33];
	int32_t iTempValue;
	int64_t iEnterSerialKey;
};

struct sP_CL2FE_REQ_PC_LOADING_COMPLETE {
	int32_t iPC_ID;
};

struct sP_CL2FE_REP_LIVE_CHECK {
	int32_t iTempValue;
};

struct sP_CL2FE_REQ_PC_MOVE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

struct sP_CL2FE_REQ_PC_STOP {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sP_CL2FE_REQ_PC_JUMP {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

struct sP_CL2FE_REQ_PC_MOVEPLATFORM {
	uint64_t iCliTime;

	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;
	int32_t iX;
	int32_t iY;
	int32_t iZ;

	float fVX;
	float fVY;
	float fVZ;

	int32_t bDown;
	uint32_t iPlatformID;
	int32_t iAngle;
	uint32_t cKeyValue;
	int32_t iSpeed;
};

struct sP_CL2FE_REQ_PC_GOTO {
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

struct sP_CL2FE_GM_REQ_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

struct sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_PC_EXIT {
	int32_t iID;
};

// ========================================================[[ ShardServer2Client packets ]]========================================================

struct sP_FE2CL_REP_PC_ENTER_SUCC {
	int32_t iID;
	sPCLoadData2CL PCLoadData2CL;
	uint64_t uiSvrTime;
};

struct sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC {
    int32_t iPC_ID;
};

struct sP_FE2CL_REQ_LIVE_CHECK {
	int32_t iTempValue;
};

// literally just a wrapper for a sPCAppearanceData struct :/
struct sP_FE2CL_PC_NEW {
	sPCAppearanceData PCAppearanceData;
};

struct sP_FE2CL_PC_MOVE {
	uint64_t iCliTime;

	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iAngle;
	uint8_t cKeyValue;

	int32_t iSpeed;

	int32_t iID;

	uint64_t iSvrTime;
};

struct sP_FE2CL_PC_STOP {
	uint64_t iCliTime;

	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iID;

	uint64_t iSvrTime;
};

struct sP_FE2CL_PC_JUMP {
	uint64_t iCliTime;

	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iID;

	uint64_t iSvrTime;
};

struct sP_FE2CL_PC_MOVEPLATFORM {
	uint64_t iCliTime;

	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;

	int32_t iX;
	int32_t iY;
	int32_t iZ;

	float fVX;
	float fVY;
	float fVZ;

	int32_t bDown;
	uint32_t iPlatformID;
	int32_t iAngle;
	int8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

struct sP_FE2CL_GM_REP_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

struct sP_FE2CL_PC_EXIT {
	int32_t iID;
	int32_t iExitType;
};

struct sP_FE2CL_REP_PC_GOTO_SUCC {
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC {
	int32_t iPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_PC_EXIT_SUCC {
	int32_t iID;
	int32_t iExitCode;
};

#pragma pack(2)
struct sP_FE2CL_PC_MOTD_LOGIN {
	int8_t iType;
	uint16_t szSystemMsg[512];
};

#pragma pack(pop)


#endif
