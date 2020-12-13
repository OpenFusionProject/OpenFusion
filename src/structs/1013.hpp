/* genstructs.py */

#define ACADEMY

#define AEQUIP_COUNT 12
#define AINVEN_COUNT 50
#define AQINVEN_COUNT 50
#define ABANK_COUNT 200

// 56 real nanos, zeroeth entry, Van Kleiss
#define NANO_COUNT 58

// Academy-specific packets
#define P_FE2CL_REP_NANO_BOOK_SUBSET 822083892
// There are more, but we don't currently implement them
// and they might all be unused anyway.

#pragma pack(push)

#pragma pack(4)
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

#pragma pack(4)
struct sOnItem {
	int16_t iEquipHandID;
	int16_t iEquipUBID;
	int16_t iEquipLBID;
	int16_t iEquipFootID;
	int16_t iEquipHeadID;
	int16_t iEquipFaceID;
	int16_t iEquipBackID;
};

#pragma pack(4)
struct sOnItem_Index {
	int16_t iEquipUBID_index;
	int16_t iEquipLBID_index;
	int16_t iEquipFootID_index;
	int16_t iFaceStyle;
	int16_t iHairStyle;
};

#pragma pack(4)
struct sPCStyle2 {
	int8_t iAppearanceFlag;
	int8_t iTutorialFlag;
	int8_t iPayzoneFlag;
};

#pragma pack(4)
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
	int32_t iSerial;
};

#pragma pack(4)
struct sItemTrade {
	int16_t iType;
	int16_t iID;
	int32_t iOpt;
	int32_t iInvenNum;
	int32_t iSlotNum;
};

#pragma pack(4)
struct sEmailItemInfoFromCL {
	int32_t iSlotNum;
	sItemBase ItemInven;
};

#pragma pack(4)
struct sTimeBuff {
	uint64_t iTimeLimit;
	uint64_t iTimeDuration;
	uint32_t iTimeRepeat;
	int32_t iValue;
	int32_t iConfirmNum;
};

#pragma pack(4)
struct sRunningQuest {
	int32_t m_aCurrTaskID;
	int m_aKillNPCID[3];
	int m_aKillNPCCount[3];
	int m_aNeededItemID[3];
	int m_aNeededItemCount[3];
};

#pragma pack(4)
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
	sItemBase aEquip[12];
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
	int aiPCSkill[33];
};

#pragma pack(4)
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
	sItemBase ItemEquip[12];
	sNano Nano;
	int32_t eRT;
};

#pragma pack(4)
struct sNPCAppearanceData {
	int32_t iNPC_ID;
	int32_t iNPCType;
	int32_t iHP;
	int32_t iConditionBitFlag;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iBarkerType;
};

#pragma pack(4)
struct sPCRegenData {
	int32_t iHP;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int16_t iActiveNanoSlotNum;
	sNano Nanos[3];
};

#pragma pack(4)
struct sPCBullet {
	int32_t eAT;
	int32_t iID;
	int32_t bCharged;
};

#pragma pack(4)
struct sBuddyBaseInfo {
	int32_t iID;
	int64_t iPCUID;
	int8_t bBlocked;
	int8_t bFreeChat;
	int8_t iPCState;
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int8_t iGender;
	int8_t iNameCheckFlag;
};

#pragma pack(4)
struct sBuddyStyleInfo {
	sPCStyle sBuddyStyle;
	sItemBase aEquip[12];
};

#pragma pack(4)
struct sNPCBullet {
	int32_t eAT;
	int32_t iID;
	int32_t bCharged;
	int32_t eST;
};

#pragma pack(4)
struct sPCRegenDataForOtherPC {
	int32_t iPC_ID;
	int32_t iHP;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iConditionBitFlag;
	int8_t iPCState;
	int8_t iSpecialState;
	sNano Nano;
};

#pragma pack(4)
struct sTransportationAppearanceData {
	int32_t eTT;
	int32_t iT_ID;
	int32_t iT_Type;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sItemReward {
	sItemBase sItem;
	int32_t eIL;
	int32_t iSlotNum;
};

#pragma pack(4)
struct sShinyAppearanceData {
	int32_t iShiny_ID;
	int32_t iShinyType;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sItemVendor {
	int32_t iVendorID;
	float fBuyCost;
	sItemBase item;
	int32_t iSortNum;
};

#pragma pack(4)
#pragma pack(4)
struct sSYSTEMTIME {
	uint32_t wYear;
	uint32_t wMonth;
	uint32_t wDayOfWeek;
	uint32_t wDay;
	uint32_t wHour;
	uint32_t wMinute;
	uint32_t wSecond;
	uint32_t wMilliseconds;
};

struct sEmailInfo {
	int64_t iEmailIndex;
	int64_t iFromPCUID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
	char16_t szSubject[32];
	int32_t iReadFlag;
	sSYSTEMTIME SendTime;
	sSYSTEMTIME DeleteTime;
	int32_t iItemCandyFlag;
};

#pragma pack(4)
struct sQuickSlot {
	int16_t iType;
	int16_t iID;
};

#pragma pack(4)
struct sItemInven {
	int16_t iType;
	int16_t iID;
	int32_t iOpt;
	int64_t iSN;
};

#pragma pack(4)
struct sTimeLimitItemDeleteInfo2CL {
	int32_t eIL;
	int32_t iSlotNum;
};

#pragma pack(2)
struct sEPRecord {
	int16_t uiScore;
	int8_t uiRank;
	int8_t uiRing;
	int16_t uiTime;
};

#pragma pack(2)
struct sNanoBank {
	int16_t iSkillID;
	int16_t iStamina;
};

#pragma pack(4)
struct sTimeBuff_Svr {
	uint64_t iTimeLimit;
	uint64_t iTimeDuration;
	int32_t iTimeRepeat;
	int32_t iValue;
	int32_t iConfirmNum;
	int16_t iTimeFlow;
};

#pragma pack(4)
struct sBulletAppearanceData {
	int32_t iBullet_ID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
};

#pragma pack(4)
struct sTransportationLoadData {
	int32_t iAISvrID;
	int32_t eTT;
	int32_t iT_Type;
	int32_t iMapType;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sAttackResult {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int8_t iHitFlag;
};

#pragma pack(4)
struct sCAttackResult {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int8_t iHitFlag;
	int16_t iActiveNanoSlotNum;
	int32_t bNanoDeactive;
	int16_t iNanoID;
	int16_t iNanoStamina;
	int32_t iConditionBitFlag;
	int32_t eCSTB___Del;
};

#pragma pack(4)
struct sSkillResult_Damage {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
};

#pragma pack(4)
struct sSkillResult_DotDamage {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int16_t iStamina;
	int32_t bNanoDeactive;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sSkillResult_Heal_HP {
	int32_t eCT;
	int32_t iID;
	int32_t iHealHP;
	int32_t iHP;
};

#pragma pack(4)
struct sSkillResult_Heal_Stamina {
	int32_t eCT;
	int32_t iID;
	int16_t iHealNanoStamina;
	sNano Nano;
};

#pragma pack(4)
struct sSkillResult_Stamina_Self {
	int32_t eCT;
	int32_t iID;
	int32_t iReduceHP;
	int32_t iHP;
	int16_t iHealNanoStamina;
	sNano Nano;
};

#pragma pack(4)
struct sSkillResult_Damage_N_Debuff {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int16_t iStamina;
	int32_t bNanoDeactive;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sSkillResult_Buff {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sSkillResult_BatteryDrain {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDrainW;
	int32_t iBatteryW;
	int32_t iDrainN;
	int32_t iBatteryN;
	int16_t iStamina;
	int32_t bNanoDeactive;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sSkillResult_Damage_N_Move {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int32_t iMoveX;
	int32_t iMoveY;
	int32_t iMoveZ;
	int32_t iBlockMove;
};

#pragma pack(4)
struct sSkillResult_Move {
	int32_t eCT;
	int32_t iID;
	int32_t iMapNum;
	int32_t iMoveX;
	int32_t iMoveY;
	int32_t iMoveZ;
};

#pragma pack(4)
struct sSkillResult_Resurrect {
	int32_t eCT;
	int32_t iID;
	int32_t iRegenHP;
};

#pragma pack(4)
struct sPC_HP {
	int32_t iPC_ID;
	int32_t iHP;
};

#pragma pack(4)
struct sPC_BATTERYs {
	int32_t iPC_ID;
	int32_t iBatteryW;
	int32_t iBatteryN;
};

#pragma pack(4)
struct sPC_NanoSlots {
	int aNanoSlots[3];
	int16_t iActiveNanoSlotNum;
};

#pragma pack(4)
struct sPC_Nano {
	int32_t iPC_ID;
	sNano Nano;
	int16_t iActiveNanoSlotNum;
};

#pragma pack(4)
struct sNPCLocationData {
	int32_t iNPC_Type;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iRoute;
};

#pragma pack(4)
struct sGroupNPCLocationData {
	int32_t iGroupType;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iRoute;
	int aGroupNPCIDs[5];
};

#pragma pack(4)
struct sPCGroupMemberInfo {
	int32_t iPC_ID;
	uint64_t iPCUID;
	int8_t iNameCheck;
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int8_t iSpecialState;
	int16_t iLv;
	int32_t iHP;
	int32_t iMaxHP;
	int32_t iMapType;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t bNano;
	sNano Nano;
};

#pragma pack(4)
struct sNPCGroupMemberInfo {
	int32_t iNPC_ID;
	int32_t iNPC_Type;
	int32_t iHP;
	int32_t iMapType;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sEPElement {
	int32_t iLID;
	int32_t iGID;
	int32_t iType;
	int32_t iTargetGID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iEnable;
	int32_t iONOFF;
};

#pragma pack(4)
struct sChannelInfo {
	int32_t iChannelNum;
	int32_t iCurrentUserCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ENTER {
	char16_t szID[33];
	int32_t iTempValue;
	int64_t iEnterSerialKey;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_EXIT {
	int32_t iID;
};

#pragma pack(4)
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

#pragma pack(4)
struct sP_CL2FE_REQ_PC_STOP {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
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

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ATTACK_NPCs {
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_REGEN {
	int32_t iRegenType;
	int32_t eIL;
	int32_t iIndex;
};

#pragma pack(4)
struct sP_CL2FE_REQ_ITEM_MOVE {
	int32_t eFrom;
	int32_t iFromSlotNum;
	int32_t eTo;
	int32_t iToSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TASK_START {
	int32_t iTaskNum;
	int32_t iNPC_ID;
	int32_t iEscortNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TASK_END {
	int32_t iTaskNum;
	int32_t iNPC_ID;
	int8_t iBox1Choice;
	int8_t iBox2Choice;
	int32_t iEscortNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_EQUIP {
	int16_t iNanoID;
	int16_t iNanoSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_UNEQUIP {
	int16_t iNanoSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_ACTIVE {
	int16_t iNanoSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_TUNE {
	int16_t iNanoID;
	int16_t iTuneID;
	int aiNeedItemSlotNum[10];
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_SKILL_USE {
	int8_t iBulletID;
	int32_t iArg1;
	int32_t iArg2;
	int32_t iArg3;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TASK_STOP {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TASK_CONTINUE {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GOTO {
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

#pragma pack(4)
struct sP_CL2FE_REQ_CHARGE_NANO_STAMINA {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_KILL_QUEST_NPCs {
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
	int32_t iInvenSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL {
	int32_t iInvenSlotNum;
	int32_t iItemCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ITEM_DELETE {
	int32_t eIL;
	int32_t iSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GIVE_ITEM {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase Item;
	int32_t iTimeLeft;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ROCKET_STYLE_READY {
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE {
	int32_t iSkillID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT {
	int8_t iBulletID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GRENADE_STYLE_READY {
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE {
	int32_t iSkillID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GRENADE_STYLE_HIT {
	int8_t iBulletID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_NANO_CREATE {
	int16_t iNanoID;
	int32_t iNeedQuestItemSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_OFFER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_OFFER_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_OFFER_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int16_t iErrorCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_CONFIRM {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_CONFIRM_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iCandy;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int8_t iFreeChatUse;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_BANK_OPEN {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_BANK_CLOSE {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_START {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
	int32_t iInvenSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_COMBAT_BEGIN {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_COMBAT_END {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_REQUEST_MAKE_BUDDY {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY {
	int8_t iAcceptFlag;
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_BUDDY_STYLE {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SET_BUDDY_BLOCK {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_REMOVE_BUDDY {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_BUDDY_STATE {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_JUMPPAD {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	uint8_t cKeyValue;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_LAUNCHER {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ZIPLINE {
	uint64_t iCliTime;
	int32_t iStX;
	int32_t iStY;
	int32_t iStZ;
	float fMovDistance;
	float fMaxDistance;
	float fDummy;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t bDown;
	int32_t iRollMax;
	uint8_t iRoll;
	int32_t iAngle;
	int32_t iSpeed;
};

#pragma pack(4)
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
	uint8_t cKeyValue;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SLOPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iSpeed;
	uint8_t cKeyValue;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iSlopeID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_STATE_CHANGE {
	int32_t iState;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_MAP_WARP {
	int32_t iMapNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GIVE_NANO {
	int16_t iNanoID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_SUMMON {
	int32_t iNPCType;
	int16_t iNPCCnt;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_UNSUMMON {
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_ITEM_CHEST_OPEN {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase ChestItem;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GIVE_NANO_SKILL {
	int16_t iNanoID;
	int16_t iNanoSkillID;
};

#pragma pack(4)
struct sP_CL2FE_DOT_DAMAGE_ONOFF {
	int32_t iFlag;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_WARP_USE_NPC {
	int32_t iNPC_ID;
	int32_t iWarpID;
	int32_t eIL1;
	int32_t iItemSlot1;
	int32_t eIL2;
	int32_t iItemSlot2;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GROUP_INVITE {
	int32_t iID_To;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE {
	int32_t iID_From;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GROUP_JOIN {
	int32_t iID_From;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_GROUP_LEAVE {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_BUDDY_WARP {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_MEMBER_STYLE {
	int32_t iMemberID;
	int64_t iMemberUID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_GROUP_STYLE {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_CHANGE_MENTOR {
	int16_t iMentor;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_BUDDY_LOCATION {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_GROUP_SUMMON {
	int32_t iNPCGroupType;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_WARP_TO_PC {
	int32_t iPC_ID;
	int32_t iPCUID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RANK_GET_LIST {
	int32_t iRankListPageNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RANK_GET_DETAIL {
	int32_t iEP_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RANK_GET_PC_INFO {
	int32_t iEP_ID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RACE_START {
	int32_t iStartEcomID;
	int32_t iEPRaceMode;
	int32_t iEPTicketItemSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RACE_END {
	int32_t iEndEcomID;
	int32_t iEPTicketItemSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_RACE_CANCEL {
	int32_t iStartEcomID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_EP_GET_RING {
	int32_t iRingLID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_IM_CHANGE_SWITCH_STATUS {
	int32_t iSwitchLID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SHINY_PICKUP {
	int32_t iShinyID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SHINY_SUMMON {
	int32_t iShinyType;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_MOVETRANSPORTATION {
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
	int32_t iT_ID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_ANY_GROUP_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iGroupPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_BARKER {
	int32_t iMissionTaskID;
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_SEND_ANY_GROUP_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iGroupPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION {
	int32_t eTT;
	int32_t iNPC_ID;
	int32_t iLocationID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION {
	int32_t iNPC_ID;
	int32_t iTransporationID;
	int32_t eIL;
	int32_t iSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH {
	int32_t iPC_ID;
	int8_t iSpecialStateFlag;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_KICK_PLAYER {
	int32_t iPC_ID;
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT {
	int32_t iPC_ID;
	int32_t eTargetPCSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
	int32_t eTeleportType;
	int32_t iToMapType;
	int32_t iToMap;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t eGoalPCSearchBy;
	int32_t iGoalPC_ID;
	char16_t szGoalPC_FirstName[10];
	char16_t szGoalPC_LastName[18];
	int64_t iGoalPC_UID;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_PC_LOCATION {
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_PC_ANNOUNCE {
	int8_t iAreaType;
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

#pragma pack(4)
struct sP_CL2FE_REQ_SET_PC_BLOCK {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_REGIST_RXCOM {
	int32_t iNPCID;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_PC_MOTD_REGISTER {
	int8_t iType;
	char16_t szSystemMsg[512];
};

#pragma pack(4)
struct sP_CL2FE_REQ_ITEM_USE {
	int32_t eIL;
	int32_t iSlotNum;
	int16_t iNanoSlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_WARP_USE_RECALL {
	int32_t iGroupMemberID;
};

#pragma pack(4)
struct sP_CL2FE_REP_LIVE_CHECK {
	int32_t iTempValue;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_MISSION_COMPLETE {
	int32_t iMissionNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TASK_COMPLETE {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_INTERACTION {
	int32_t iNPC_ID;
	int32_t bFlag;
};

#pragma pack(4)
struct sP_CL2FE_DOT_HEAL_ONOFF {
	int32_t iFlag;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH {
	int32_t iPC_ID;
	int8_t iSpecialStateFlag;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK {
	int32_t iTempData;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_READ_EMAIL {
	int64_t iEmailIndex;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST {
	int8_t iPageNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_DELETE_EMAIL {
	int64_t iEmailIndexArray[5];
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SEND_EMAIL {
	int64_t iTo_PCUID;
	char16_t szSubject[32];
	char16_t szContent[512];
	sEmailItemInfoFromCL aItem[4];
	int32_t iCash;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY {
	int64_t iEmailIndex;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF {
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
	int32_t iONOFF;
	int8_t iSpecialStateFlag;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID {
	int32_t iCurrentMissionID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_GROUP_INVITE {
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_GROUP_KICK {
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_FIRST_USE_FLAG_SET {
	int32_t iFlagCode;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TRANSPORT_WARP {
	int32_t iTransport_ID;
	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_TIME_TO_GO_WARP {
	int32_t iNPC_ID;
	int32_t iWarpID;
	int32_t eIL1;
	int32_t iItemSlot1;
	int32_t eIL2;
	int32_t iItemSlot2;
	int32_t iPC_Level;
	int32_t iPayFlag;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL {
	int64_t iEmailIndex;
};

#pragma pack(4)
struct sP_CL2FE_REQ_CHANNEL_INFO {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_CHANNEL_NUM {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_WARP_CHANNEL {
	int32_t iChannelNum;
	int8_t iWarpType;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_LOADING_COMPLETE {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY {
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY {
	int32_t iAcceptFlag;
	int64_t iBuddyPCUID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ATTACK_CHARs {
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_READY {
	int32_t iStreetStallItemInvenSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_CANCEL {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_REGIST_ITEM {
	int32_t iItemListNum;
	int32_t iItemInvenSlotNum;
	sItemBase Item;
	int32_t iPrice;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_UNREGIST_ITEM {
	int32_t iItemListNum;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_SALE_START {
	int32_t iStreetStallItemInvenSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_ITEM_LIST {
	int32_t iStreetStallPC_ID;
};

#pragma pack(4)
struct sP_CL2FE_PC_STREETSTALL_REQ_ITEM_BUY {
	int32_t iStreetStallPC_ID;
	int32_t iItemListNum;
	int32_t iEmptyInvenSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ITEM_COMBINATION {
	int32_t iCostumeItemSlot;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_SET_PC_SKILL {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SKILL_ADD {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iSkillItemInvenSlotNum;
	int32_t iPreSkillSlotNum;
	int32_t iPreSkillID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SKILL_DEL {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_SKILL_USE {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iMoveFlag;
	int32_t iFromX;
	int32_t iFromY;
	int32_t iFromZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iMainTargetType;
	int32_t iMainTargetID;
	int32_t iTargetLocationX;
	int32_t iTargetLocationY;
	int32_t iTargetLocationZ;
	int32_t iTargetCount;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ROPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	uint32_t iRopeID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_BELT {
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
	uint32_t iBeltID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VEHICLE_ON {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_VEHICLE_OFF {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_REGIST_QUICK_SLOT {
	int32_t iSlotNum;
	int16_t iItemType;
	int16_t iItemID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_DISASSEMBLE_ITEM {
	int32_t iItemSlot;
};

#pragma pack(4)
struct sP_CL2FE_GM_REQ_REWARD_RATE {
	int32_t iGetSet;
	int32_t iRewardType;
	int32_t iRewardRateIndex;
	int32_t iSetRateValue;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ITEM_ENCHANT {
	int32_t iEnchantItemSlot;
	int32_t iWeaponMaterialItemSlot;
	int32_t iDefenceMaterialItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

#pragma pack(4)
struct sP_CL2FE_TEXT_MESSAGE {
	char16_t wszText[128];
};

#pragma pack(4)
struct sP_CL2FE_REQ_PERSIST_CHAR {
	int32_t PCID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_RELOAD_CHAR {
	int32_t PCID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_AI_SCRIPT {
	int32_t PCID;
	int32_t Command;
	int32_t MobID;
	int32_t InstanceID;
	char16_t ScriptName[260];
};

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

#pragma pack(4)
struct sP_CL2LS_REQ_CHECK_CHAR_NAME {
	int32_t iFNCode;
	int32_t iLNCode;
	int32_t iMNCode;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2LS_REQ_SAVE_CHAR_NAME {
	int8_t iSlotNum;
	int8_t iGender;
	int32_t iFNCode;
	int32_t iLNCode;
	int32_t iMNCode;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2LS_REQ_CHAR_CREATE {
	sPCStyle PCStyle;
	sOnItem sOn_Item;
	sOnItem_Index sOn_Item_Index;
};

#pragma pack(4)
struct sP_CL2LS_REQ_CHAR_SELECT {
	int64_t iPC_UID;
};

#pragma pack(4)
struct sP_CL2LS_REQ_CHAR_DELETE {
	int64_t iPC_UID;
};

#pragma pack(4)
struct sP_CL2LS_REQ_SHARD_SELECT {
	int8_t ShardNum;
};

#pragma pack(4)
struct sP_CL2LS_REQ_SHARD_LIST_INFO {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2LS_CHECK_NAME_LIST {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
};

#pragma pack(4)
struct sP_CL2LS_REQ_SAVE_CHAR_TUTOR {
	int64_t iPC_UID;
	int8_t iTutorialFlag;
};

#pragma pack(4)
struct sP_CL2LS_REQ_PC_EXIT_DUPLICATE {
	char16_t szID[33];
	char16_t szPassword[33];
};

#pragma pack(4)
struct sP_CL2LS_REP_LIVE_CHECK {
	int32_t iTempValue;
};

#pragma pack(4)
struct sP_CL2LS_REQ_CHANGE_CHAR_NAME {
	int64_t iPCUID;
	int8_t iSlotNum;
	int8_t iGender;
	int32_t iFNCode;
	int32_t iLNCode;
	int32_t iMNCode;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_CL2LS_REQ_SERVER_SELECT {
	int8_t ServerNum;
};

#pragma pack(4)
struct sPacket {
	uint32_t dwType;
	uint8_t szData[4096];
};

#pragma pack(4)
struct sPacket_Full {
	uint32_t dwSize;
	uint32_t dwType;
	uint8_t szData[4096];
};

#pragma pack(4)
struct sPacket2x {
	uint32_t dwType;
	uint8_t szData[8192];
};

#pragma pack(4)
struct sPacket2x_Full {
	uint32_t dwSize;
	uint32_t dwType;
	uint8_t szData[8192];
};

#pragma pack(4)
struct sP_FE2CL_ERROR {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ENTER_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ENTER_SUCC {
	int32_t iID;
	sPCLoadData2CL PCLoadData2CL;
	uint64_t uiSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_NEW {
	sPCAppearanceData PCAppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_EXIT_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_EXIT_SUCC {
	int32_t iID;
	int32_t iExitCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_EXIT {
	int32_t iID;
	int32_t iExitType;
};

#pragma pack(4)
struct sP_FE2CL_PC_AROUND {
	int32_t iPCCnt;
};

#pragma pack(4)
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

#pragma pack(4)
struct sP_FE2CL_PC_STOP {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iID;
	uint64_t iSvrTime;
};

#pragma pack(4)
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

#pragma pack(4)
struct sP_FE2CL_NPC_ENTER {
	sNPCAppearanceData NPCAppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_NPC_EXIT {
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_FE2CL_NPC_MOVE {
	int32_t iNPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
	int16_t iMoveStyle;
};

#pragma pack(4)
struct sP_FE2CL_NPC_NEW {
	sNPCAppearanceData NPCAppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_NPC_AROUND {
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_AROUND_DEL_PC {
	int32_t iPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_AROUND_DEL_NPC {
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC {
	int32_t iPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_ATTACK_NPCs_SUCC {
	int32_t iBatteryW;
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_ATTACK_NPCs {
	int32_t iPC_ID;
	int32_t iNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_NPC_ATTACK_PCs {
	int32_t iNPC_ID;
	int32_t iPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGEN_SUCC {
	sPCRegenData PCRegenData;
	int32_t bMoveLocation;
	int32_t iFusionMatter;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC {
	int32_t iPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_ITEM_MOVE_SUCC {
	int32_t eFrom;
	int32_t iFromSlotNum;
	sItemBase FromSlotItem;
	int32_t eTo;
	int32_t iToSlotNum;
	sItemBase ToSlotItem;
};

#pragma pack(4)
struct sP_FE2CL_PC_EQUIP_CHANGE {
	int32_t iPC_ID;
	int32_t iEquipSlotNum;
	sItemBase EquipSlotItem;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_START_SUCC {
	int32_t iTaskNum;
	int32_t iRemainTime;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_START_FAIL {
	int32_t iTaskNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_END_SUCC {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_END_FAIL {
	int32_t iTaskNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_READY {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_FIRE {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_HIT {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
	int32_t eST;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_CORRUPTION_READY {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int16_t iStyle;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_CORRUPTION_HIT {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int16_t iStyle;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_NPC_SKILL_CANCEL {
	int32_t iNPC_ID;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_EQUIP_SUCC {
	int16_t iNanoID;
	int16_t iNanoSlotNum;
	int32_t bNanoDeactive;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_UNEQUIP_SUCC {
	int16_t iNanoSlotNum;
	int32_t bNanoDeactive;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_ACTIVE_SUCC {
	int16_t iActiveNanoSlotNum;
	int32_t eCSTB___Add;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_TUNE_SUCC {
	int16_t iNanoID;
	int16_t iSkillID;
	int32_t iPC_FusionMatter;
	int aiItemSlotNum[10];
	sItemBase aItem[10];
};

#pragma pack(4)
struct sP_FE2CL_NANO_ACTIVE {
	int32_t iPC_ID;
	sNano Nano;
	int32_t iConditionBitFlag;
	int32_t eCSTB___Add;
};

#pragma pack(4)
struct sP_FE2CL_NANO_SKILL_USE_SUCC {
	int32_t iPC_ID;
	int8_t iBulletID;
	int16_t iSkillID;
	int32_t iArg1;
	int32_t iArg2;
	int32_t iArg3;
	int32_t bNanoDeactive;
	int16_t iNanoID;
	int16_t iNanoStamina;
	int32_t eST;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_NANO_SKILL_USE {
	int32_t iPC_ID;
	int8_t iBulletID;
	int16_t iSkillID;
	int32_t iArg1;
	int32_t iArg2;
	int32_t iArg3;
	int32_t bNanoDeactive;
	int16_t iNanoID;
	int16_t iNanoStamina;
	int32_t eST;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_STOP_SUCC {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_STOP_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_CONTINUE_SUCC {
	int32_t iTaskNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TASK_CONTINUE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_GOTO_SUCC {
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sP_FE2CL_REP_CHARGE_NANO_STAMINA {
	int32_t iBatteryN;
	int16_t iNanoID;
	int16_t iNanoStamina;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TICK {
	int32_t iHP;
	sNano aNano[3];
	int32_t iBatteryN;
	int32_t bResetMissionFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC {
	int32_t iNPCID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
	sItemBase ItemStay;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_DELETE_SUCC {
	int32_t eIL;
	int32_t iSlotNum;
};

#pragma pack(4)
struct sP_FE2CL_PC_ROCKET_STYLE_READY {
	int32_t iPC_ID;
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ROCKET_STYLE_FIRE_SUCC {
	int32_t iSkillID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iBatteryW;
	int32_t bNanoDeactive;
	int16_t iNanoID;
	int16_t iNanoStamina;
};

#pragma pack(4)
struct sP_FE2CL_PC_ROCKET_STYLE_FIRE {
	int32_t iPC_ID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t bNanoDeactive;
};

#pragma pack(4)
struct sP_FE2CL_PC_ROCKET_STYLE_HIT {
	int32_t iPC_ID;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_GRENADE_STYLE_READY {
	int32_t iPC_ID;
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC {
	int32_t iSkillID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iBatteryW;
	int32_t bNanoDeactive;
	int16_t iNanoID;
	int16_t iNanoStamina;
};

#pragma pack(4)
struct sP_FE2CL_PC_GRENADE_STYLE_FIRE {
	int32_t iPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t bNanoDeactive;
};

#pragma pack(4)
struct sP_FE2CL_PC_GRENADE_STYLE_HIT {
	int32_t iPC_ID;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_OFFER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_OFFER_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_OFFER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_OFFER_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int16_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CONFIRM {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item[12];
	int32_t iCandy;
	sItemTrade ItemStay[12];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CONFIRM_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade TradeItem;
	sItemTrade InvenItem;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade TradeItem;
	sItemTrade InvenItem;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iCandy;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_NANO_CREATE_SUCC {
	int32_t iPC_FusionMatter;
	int32_t iQuestItemSlotNum;
	sItemBase QuestItem;
	sNano Nano;
	int16_t iPC_Level;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_NANO_CREATE_FAIL {
	int32_t iPC_ID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_TUNE_FAIL {
	int32_t iPC_ID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BANK_OPEN_SUCC {
	sItemBase aBank[200];
	int32_t iExtraBank;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BANK_OPEN_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BANK_CLOSE_SUCC {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BANK_CLOSE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_START_SUCC {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_START_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC {
	sItemVendor item[20];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT {
	int32_t eCT;
	int32_t iID;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_GIVE_ITEM_SUCC {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase Item;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_GIVE_ITEM_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC {
	int32_t iID;
	int64_t iPCUID;
	int8_t iListNum;
	int8_t iBuddyCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BUDDYLIST_INFO_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC {
	int32_t iRequestID;
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

#pragma pack(4)
struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC {
	int8_t iBuddySlot;
	sBuddyBaseInfo BuddyInfo;
};

#pragma pack(4)
struct sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC {
	int64_t iFromPCUID;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC {
	int64_t iFromPCUID;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_STYLE_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
	sBuddyStyleInfo sBuddyStyle;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_STYLE_FAIL {
	int32_t iErrorCode;
	int64_t iBuddyPCUID;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_STATE_SUCC {
	int aBuddyID[50];
	int8_t aBuddyState[50];
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_STATE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_FE2CL_REP_SET_BUDDY_BLOCK_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_REMOVE_BUDDY_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_FE2CL_REP_REMOVE_BUDDY_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_JUMPPAD {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_LAUNCHER {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
	int32_t iAngle;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_ZIPLINE {
	uint64_t iCliTime;
	int32_t iStX;
	int32_t iStY;
	int32_t iStZ;
	float fMovDistance;
	float fMaxDistance;
	float fDummy;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t bDown;
	int32_t iRollMax;
	uint8_t iRoll;
	int32_t iAngle;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
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
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_SLOPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iSpeed;
	uint8_t cKeyValue;
	int32_t iPC_ID;
	uint64_t iSvrTime;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iSlopeID;
};

#pragma pack(4)
struct sP_FE2CL_PC_STATE_CHANGE {
	int32_t iPC_ID;
	int8_t iState;
};

#pragma pack(4)
struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER {
	int32_t iRequestID;
	int32_t iBuddyID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_FE2CL_REP_REWARD_ITEM {
	int32_t m_iCandy;
	int32_t m_iFusionMatter;
	int32_t m_iBatteryN;
	int32_t m_iBatteryW;
	int8_t iItemCnt;
	int32_t iFatigue;
	int32_t iFatigue_Level;
	int32_t iNPC_TypeID;
	int32_t iTaskID;
};

#pragma pack(4)
struct sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC {
	int32_t iSlotNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_ITEM_CHEST_OPEN_FAIL {
	int32_t iSlotNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK {
	int32_t eCT;
	int32_t iID;
	int16_t iTB_ID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC {
	int32_t iCandy;
	int32_t iBatteryW;
	int32_t iBatteryN;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_NPC_ROCKET_STYLE_FIRE {
	int32_t iNPC_ID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sNPCBullet Bullet;
};

#pragma pack(4)
struct sP_FE2CL_NPC_GRENADE_STYLE_FIRE {
	int32_t iNPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sNPCBullet Bullet;
};

#pragma pack(4)
struct sP_FE2CL_NPC_BULLET_STYLE_HIT {
	int32_t iNPC_ID;
	int8_t iBulletID;
	sNPCBullet Bullet;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_CHARACTER_ATTACK_CHARACTERs {
	int32_t eCT;
	int32_t iCharacterID;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_INVITE {
	int32_t iHostID;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_INVITE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_INVITE_REFUSE {
	int32_t iID_To;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_JOIN {
	int32_t iID_NewMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_JOIN_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_JOIN_SUCC {
	int32_t iID_NewMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_LEAVE {
	int32_t iID_LeaveMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_LEAVE_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_LEAVE_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_MEMBER_INFO {
	int32_t iID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC {
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t eIL;
	int32_t iItemSlotNum;
	sItemBase Item;
	int32_t iCandy;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_USE_NPC_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC {
	int16_t iMentor;
	int16_t iMentorCnt;
	int32_t iFusionMatter;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_CHANGE_MENTOR_FAIL {
	int16_t iMentor;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_MEMBER_STYLE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_MEMBER_STYLE_SUCC {
	int32_t iMemberID;
	int64_t iMemberUID;
	sBuddyStyleInfo BuddyStyleInfo;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_GROUP_STYLE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_GROUP_STYLE_SUCC {
	int32_t iMemberCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_REGEN {
	sPCRegenDataForOtherPC PCRegenDataForOtherPC;
};

#pragma pack(4)
struct sP_FE2CL_INSTANCE_MAP_INFO {
	int32_t iInstanceMapNum;
	uint64_t iCreateTick;
	int32_t iMapCoordX_Min;
	int32_t iMapCoordX_Max;
	int32_t iMapCoordY_Min;
	int32_t iMapCoordY_Max;
	int32_t iMapCoordZ_Min;
	int32_t iMapCoordZ_Max;
	int32_t iEP_ID;
	int32_t iEPTopRecord_Score;
	int32_t iEPTopRecord_Rank;
	int32_t iEPTopRecord_Time;
	int32_t iEPTopRecord_RingCount;
	int32_t iEPSwitch_StatusON_Cnt;
};

#pragma pack(4)
struct sP_FE2CL_TRANSPORTATION_ENTER {
	sTransportationAppearanceData AppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_TRANSPORTATION_EXIT {
	int32_t eTT;
	int32_t iT_ID;
};

#pragma pack(4)
struct sP_FE2CL_TRANSPORTATION_MOVE {
	int32_t eTT;
	int32_t iT_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
	int16_t iMoveStyle;
};

#pragma pack(4)
struct sP_FE2CL_TRANSPORTATION_NEW {
	sTransportationAppearanceData AppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_TRANSPORTATION_AROUND {
	int32_t iCnt;
};

#pragma pack(4)
struct sP_FE2CL_AROUND_DEL_TRANSPORTATION {
	int32_t eTT;
	int32_t iCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RANK_LIST {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RANK_DETAIL {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RANK_PC_INFO {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_START_SUCC {
	uint64_t iStartTick;
	int32_t iLimitTime;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_START_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_END_SUCC {
	int32_t iEPRaceMode;
	int32_t iEPRaceTime;
	int32_t iEPRingCnt;
	int32_t iEPScore;
	int32_t iEPRank;
	int32_t iEPRewardFM;
	int32_t iEPTopScore;
	int32_t iEPTopRank;
	int32_t iEPTopTime;
	int32_t iEPTopRingCount;
	int32_t iFusionMatter;
	sItemReward RewardItem;
	int32_t iFatigue;
	int32_t iFatigue_Level;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_END_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_CANCEL_SUCC {
	int32_t iTemp;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_CANCEL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_GET_RING_SUCC {
	int32_t iRingLID;
	int32_t iRingCount_Get;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_GET_RING_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_IM_CHANGE_SWITCH_STATUS {
	int32_t iMapNum;
	int32_t iSwitchLID;
	int32_t iSwitchGID;
	int32_t iSwitchStatus;
};

#pragma pack(4)
struct sP_FE2CL_SHINY_ENTER {
	sShinyAppearanceData ShinyAppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_SHINY_EXIT {
	int32_t iShinyID;
};

#pragma pack(4)
struct sP_FE2CL_SHINY_NEW {
	sShinyAppearanceData ShinyAppearanceData;
};

#pragma pack(4)
struct sP_FE2CL_SHINY_AROUND {
	int32_t iShinyCnt;
};

#pragma pack(4)
struct sP_FE2CL_AROUND_DEL_SHINY {
	int32_t iShinyCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_SHINY_PICKUP_FAIL {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_SHINY_PICKUP_SUCC {
	int16_t iSkillID;
	int32_t eCSTB;
};

#pragma pack(4)
struct sP_FE2CL_PC_MOVETRANSPORTATION {
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
	int32_t iT_ID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_BARKER {
	int32_t iNPC_ID;
	int32_t iMissionStringID;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL {
	int32_t eTT;
	int32_t iLocationID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC {
	int32_t eTT;
	int32_t iLocationID;
	int32_t iWarpLocationFlag;
	int64_t aWyvernLocationFlag[2];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL {
	int32_t iTransportationID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC {
	int32_t eTT;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iCandy;
};

#pragma pack(4)
struct sP_FE2CL_ANNOUNCE_MSG {
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC {
	int32_t iPC_ID;
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

#pragma pack(4)
struct sP_FE2CL_PC_SPECIAL_STATE_CHANGE {
	int32_t iPC_ID;
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

#pragma pack(4)
struct sP_FE2CL_GM_REP_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

#pragma pack(4)
struct sP_FE2CL_GM_PC_CHANGE_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

#pragma pack(4)
struct sP_FE2CL_GM_REP_PC_LOCATION {
	int64_t iTargetPC_UID;
	int32_t iTargetPC_ID;
	int32_t iShardID;
	int32_t iMapType;
	int32_t iMapID;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
};

#pragma pack(4)
struct sP_FE2CL_GM_REP_PC_ANNOUNCE {
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BUDDY_WARP_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_CHANGE_LEVEL {
	int32_t iPC_ID;
	int16_t iPC_Level;
};

#pragma pack(4)
struct sP_FE2CL_REP_SET_PC_BLOCK_SUCC {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
	int8_t iBuddySlot;
};

#pragma pack(4)
struct sP_FE2CL_REP_SET_PC_BLOCK_FAIL {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_REGIST_RXCOM {
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

#pragma pack(4)
struct sP_FE2CL_REP_REGIST_RXCOM_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_INVEN_FULL_MSG {
	int8_t iType;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REQ_LIVE_CHECK {
	int32_t iTempValue;
};

#pragma pack(4)
struct sP_FE2CL_PC_MOTD_LOGIN {
	int8_t iType;
	char16_t szSystemMsg[512];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_USE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_USE_SUCC {
	int32_t iPC_ID;
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase RemainItem;
	int16_t iSkillID;
	int32_t eST;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_ITEM_USE {
	int32_t iPC_ID;
	int16_t iSkillID;
	int32_t eST;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_LOCATION_SUCC {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int8_t iShardNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_GET_BUDDY_LOCATION_FAIL {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RIDING_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RIDING_SUCC {
	int32_t iPC_ID;
	int32_t eRT;
};

#pragma pack(4)
struct sP_FE2CL_PC_RIDING {
	int32_t iPC_ID;
	int32_t eRT;
};

#pragma pack(4)
struct sP_FE2CL_PC_BROOMSTICK_MOVE {
	int32_t iPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BUDDY_WARP_OTHER_SHARD_SUCC {
	int64_t iBuddyPCUID;
	int8_t iShardNum;
	int32_t iChannelNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_WARP_USE_RECALL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_EXIT_DUPLICATE {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_MISSION_COMPLETE_SUCC {
	int32_t iMissionNum;
};

#pragma pack(4)
struct sP_FE2CL_PC_BUFF_UPDATE {
	int32_t eCSTB;
	int32_t eTBU;
	int32_t eTBT;
	sTimeBuff TimeBuff;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_NEW_EMAIL {
	int32_t iNewEmailCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_READ_EMAIL_SUCC {
	int64_t iEmailIndex;
	char16_t szContent[512];
	sItemBase aItem[4];
	int32_t iCash;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_READ_EMAIL_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC {
	int8_t iPageNum;
	sEmailInfo aEmailInfo[5];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_FAIL {
	int8_t iPageNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC {
	int64_t iEmailIndexArray[5];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_DELETE_EMAIL_FAIL {
	int64_t iEmailIndexArray[5];
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SEND_EMAIL_SUCC {
	int64_t iTo_PCUID;
	int32_t iCandy;
	sEmailItemInfoFromCL aItem[4];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SEND_EMAIL_FAIL {
	int64_t iTo_PCUID;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_FAIL {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC {
	int64_t iEmailIndex;
	int32_t iCandy;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_SUDDEN_DEAD {
	int32_t iPC_ID;
	int32_t iSuddenDeadReason;
	int32_t iDamage;
	int32_t iHP;
};

#pragma pack(4)
struct sP_FE2CL_REP_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF_SUCC {
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID {
	int32_t iCurrentMissionID;
};

#pragma pack(4)
struct sP_FE2CL_REP_NPC_GROUP_INVITE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_NPC_GROUP_INVITE_SUCC {
	int32_t iPC_ID;
	int32_t iNPC_ID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_NPC_GROUP_KICK_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_NPC_GROUP_KICK_SUCC {
	int32_t iPC_ID;
	int32_t iNPC_ID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_EVENT {
	int32_t iPC_ID;
	int32_t iEventID;
	int32_t iEventValue1;
	int32_t iEventValue2;
	int32_t iEventValue3;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRANSPORT_WARP_SUCC {
	sTransportationAppearanceData TransportationAppearanceData;
	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC {
	int64_t iEmailIndex;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC {
	int32_t iPC_ID;
};

#pragma pack(4)
struct sP_FE2CL_REP_CHANNEL_INFO {
	int32_t iCurrChannelNum;
	int32_t iChannelCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_CHANNEL_NUM {
	int32_t iChannelNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_CHANNEL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_WARP_CHANNEL_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC {
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int64_t iPCUID;
	int8_t iNameCheckFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_FAIL {
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_FIND_NAME_ACCEPT_BUDDY_FAIL {
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int64_t iPCUID;
	int8_t iNameCheckFlag;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_BUDDY_WARP_SAME_SHARD_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_ATTACK_CHARs_SUCC {
	int32_t iBatteryW;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_ATTACK_CHARs {
	int32_t iPC_ID;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_NPC_ATTACK_CHARs {
	int32_t iNPC_ID;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_CHANGE_LEVEL_SUCC {
	int32_t iLevel;
	int32_t iFusionMatter;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_NANO_CREATE {
	int32_t iPC_ID;
	int16_t iNanoID;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_READY_SUCC {
	int32_t iStreetStallItemInvenSlotNum;
	int32_t iItemListCountMax;
	float fTaxPercentage;
	int8_t iPCCharState;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_READY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_CANCEL_SUCC {
	int8_t iPCCharState;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_CANCEL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_SUCC {
	int32_t iItemListNum;
	int32_t iItemInvenSlotNum;
	sItemBase Item;
	int32_t iPrice;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_SUCC {
	int32_t iItemListNum;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_SALE_START_SUCC {
	int32_t iStreetStallItemInvenSlotNum;
	sItemBase OpenItem;
	int32_t ePCCharState;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_SALE_START_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST {
	int32_t iStreetStallPC_ID;
	int32_t iItemListCount;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_BUYER {
	int32_t iStreetStallPC_ID;
	int32_t iPC_Candy;
	int32_t iPC_ItemInvenSlotNum;
	sItemBase PC_Item;
	int32_t iItemListNum;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_SELLER {
	int32_t iBuyerPC_ID;
	int32_t iStreetStallPC_Candy;
	int32_t iStreetStallPC_ItemInvenSlotNum;
	sItemBase StreetStallPC_Item;
	int32_t iItemListNum;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC {
	int32_t iNewItemSlot;
	sItemBase sNewItem;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
	int32_t iCandy;
	int32_t iSuccessFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL {
	int32_t iErrorCode;
	int32_t iCostumeItemSlot;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

#pragma pack(4)
struct sP_FE2CL_PC_CASH_BUFF_UPDATE {
	int32_t eCSTB;
	int32_t eTBU;
	sTimeBuff TimeBuff;
	int32_t iConditionBitFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_ADD_SUCC {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iSkillItemInvenSlotNum;
	sItemBase SkillItem;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_ADD_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_DEL_SUCC {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_DEL_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_USE_SUCC {
	int32_t iPC_ID;
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iBlockMove;
	int32_t eST;
	int32_t iTargetID;
	int32_t iTargetType;
	int32_t iTargetLocationX;
	int32_t iTargetLocationY;
	int32_t iTargetLocationZ;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_SKILL_USE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_SKILL_USE {
	int32_t iPC_ID;
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iBlockMove;
	int32_t eST;
	int32_t iTargetID;
	int32_t iTargetType;
	int32_t iTargetLocationX;
	int32_t iTargetLocationY;
	int32_t iTargetLocationZ;
	int32_t iTargetCnt;
};

#pragma pack(4)
struct sP_FE2CL_PC_ROPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	uint32_t iRopeID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_BELT {
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
	uint32_t iBeltID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_ON_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_ON_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_OFF_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_OFF_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_PC_QUICK_SLOT_INFO {
	sQuickSlot aQuickSlot[8];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_SUCC {
	int32_t iSlotNum;
	int16_t iItemType;
	int16_t iItemID;
};

#pragma pack(4)
struct sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM {
	int32_t iItemListCount;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_SUCC {
	int32_t iNewItemSlot;
	sItemBase sNewItem;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_FAIL {
	int32_t iErrorCode;
	int32_t iItemSlot;
};

#pragma pack(4)
struct sP_FE2CL_GM_REP_REWARD_RATE_SUCC {
	float afRewardRate_Taros[5];
	float afRewardRate_FusionMatter[5];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_ENCHANT_SUCC {
	int32_t iEnchantItemSlot;
	sItemBase sEnchantItem;
	int32_t iWeaponMaterialItemSlot;
	sItemBase sWeaponMaterialItem;
	int32_t iDefenceMaterialItemSlot;
	sItemBase sDefenceMaterialItem;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
	int32_t iCandy;
	int32_t iSuccessFlag;
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_ENCHANT_FAIL {
	int32_t iErrorCode;
	int32_t iEnchantItemSlot;
	int32_t iWeaponMaterialItemSlot;
	int32_t iDefenceMaterialItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

#pragma pack(4)
struct sP_FE2CL_TEXT_MESSAGE {
	char16_t wszText[128];
};

#pragma pack(4)
struct sP_FE2CL_REP_PERSIST_CHAR {
	int32_t ResultCode;
	int32_t PCID;
};

#pragma pack(4)
struct sP_FE2CL_REP_RELOAD_CHAR {
	int32_t ResultCode;
	int32_t PCID;
	int64_t PCUID;
	sItemInven aEquip[12];
	sItemInven aInven[50];
	sItemInven aQInven[50];
	sItemInven aBank[200];
	sNano aNanoBank[37];
	sQuickSlot aQuickSlot[8];
	int16_t aNanoSlots[3];
	int16_t iActiveNanoSlotNum;
};

#pragma pack(4)
struct sP_FE2CL_REP_NANO_BOOK_SUBSET {
	int64_t PCUID;
	int32_t bookSize;
	int32_t elementOffset;
	sNano element[10];
};

#pragma pack(4)
struct sP_LS2CL_REP_LOGIN_SUCC {
	int8_t iCharCount;
	int8_t iSlotNum;
	int8_t iPaymentFlag;
	int8_t iTempForPacking4;
	uint64_t uiSvrTime;
	char16_t szID[33];
	int32_t iOpenBetaFlag;
	int32_t iChatEnabled;
};

#pragma pack(4)
struct sP_LS2CL_REP_LOGIN_FAIL {
	int32_t iErrorCode;
	char16_t szID[33];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_INFO {
	int8_t iSlot;
	int16_t iLevel;
	sPCStyle sPC_Style;
	sPCStyle2 sPC_Style2;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	sItemBase aEquip[12];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC {
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC {
	int64_t iPC_UID;
	int8_t iSlotNum;
	int8_t iGender;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_LS2CL_REP_SAVE_CHAR_NAME_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_CREATE_SUCC {
	int16_t iLevel;
	sPCStyle sPC_Style;
	sPCStyle2 sPC_Style2;
	sOnItem sOn_Item;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_CREATE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_SELECT_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_SELECT_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_DELETE_SUCC {
	int8_t iSlotNum;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_DELETE_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_SHARD_SELECT_SUCC {
	uint8_t g_FE_ServerIP[16];
	int32_t g_FE_ServerPort;
	int64_t iEnterSerialKey;
};

#pragma pack(4)
struct sP_LS2CL_REP_SHARD_SELECT_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_VERSION_CHECK_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_LS2CL_REP_VERSION_CHECK_FAIL {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHECK_NAME_LIST_SUCC {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
	int64_t aNameCodeFlag[8];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHECK_NAME_LIST_FAIL {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_PC_EXIT_DUPLICATE {
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REQ_LIVE_CHECK {
	int32_t iTempValue;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC {
	int64_t iPC_UID;
	int8_t iSlotNum;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHANGE_CHAR_NAME_FAIL {
	int64_t iPC_UID;
	int8_t iSlotNum;
	int32_t iErrorCode;
};

#pragma pack(4)
struct sP_LS2CL_REP_SHARD_LIST_INFO_SUCC {
	int8_t aShardConnectFlag[26];
};

#pragma pack(pop)

static_assert(sizeof(sP_CL2FE_REQ_PC_ENTER) == 80 || sizeof(sP_CL2FE_REQ_PC_ENTER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_EXIT) == 4 || sizeof(sP_CL2FE_REQ_PC_EXIT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_MOVE) == 44 || sizeof(sP_CL2FE_REQ_PC_MOVE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_STOP) == 20 || sizeof(sP_CL2FE_REQ_PC_STOP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_JUMP) == 44 || sizeof(sP_CL2FE_REQ_PC_JUMP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs) == 4 || sizeof(sP_CL2FE_REQ_PC_ATTACK_NPCs) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE) == 260 || sizeof(sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE) == 260 || sizeof(sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_REGEN) == 12 || sizeof(sP_CL2FE_REQ_PC_REGEN) == 0);
static_assert(sizeof(sP_CL2FE_REQ_ITEM_MOVE) == 16 || sizeof(sP_CL2FE_REQ_ITEM_MOVE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TASK_START) == 12 || sizeof(sP_CL2FE_REQ_PC_TASK_START) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TASK_END) == 16 || sizeof(sP_CL2FE_REQ_PC_TASK_END) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NANO_EQUIP) == 4 || sizeof(sP_CL2FE_REQ_NANO_EQUIP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NANO_UNEQUIP) == 2 || sizeof(sP_CL2FE_REQ_NANO_UNEQUIP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NANO_ACTIVE) == 2 || sizeof(sP_CL2FE_REQ_NANO_ACTIVE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NANO_TUNE) == 44 || sizeof(sP_CL2FE_REQ_NANO_TUNE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NANO_SKILL_USE) == 20 || sizeof(sP_CL2FE_REQ_NANO_SKILL_USE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TASK_STOP) == 4 || sizeof(sP_CL2FE_REQ_PC_TASK_STOP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TASK_CONTINUE) == 4 || sizeof(sP_CL2FE_REQ_PC_TASK_CONTINUE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GOTO) == 12 || sizeof(sP_CL2FE_REQ_PC_GOTO) == 0);
static_assert(sizeof(sP_CL2FE_REQ_CHARGE_NANO_STAMINA) == 4 || sizeof(sP_CL2FE_REQ_CHARGE_NANO_STAMINA) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_KILL_QUEST_NPCs) == 4 || sizeof(sP_CL2FE_REQ_PC_KILL_QUEST_NPCs) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY) == 32 || sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL) == 8 || sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE) == 8 || sizeof(sP_CL2FE_REQ_PC_ITEM_DELETE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM) == 28 || sizeof(sP_CL2FE_REQ_PC_GIVE_ITEM) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_READY) == 4 || sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_READY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE) == 28 || sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT) == 20 || sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_READY) == 4 || sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_READY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE) == 16 || sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_HIT) == 20 || sizeof(sP_CL2FE_REQ_PC_GRENADE_STYLE_HIT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_NANO_CREATE) == 8 || sizeof(sP_CL2FE_REQ_PC_NANO_CREATE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_CANCEL) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_CANCEL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ABORT) == 16 || sizeof(sP_CL2FE_REQ_PC_TRADE_OFFER_ABORT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_ABORT) == 12 || sizeof(sP_CL2FE_REQ_PC_TRADE_CONFIRM_ABORT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER) == 28 || sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER) == 28 || sizeof(sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER) == 16 || sizeof(sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT) == 276 || sizeof(sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_BANK_OPEN) == 4 || sizeof(sP_CL2FE_REQ_PC_BANK_OPEN) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_BANK_CLOSE) == 4 || sizeof(sP_CL2FE_REQ_PC_BANK_CLOSE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_START) == 8 || sizeof(sP_CL2FE_REQ_PC_VENDOR_START) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE) == 8 || sizeof(sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY) == 32 || sizeof(sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_COMBAT_BEGIN) == 4 || sizeof(sP_CL2FE_REQ_PC_COMBAT_BEGIN) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_COMBAT_END) == 4 || sizeof(sP_CL2FE_REQ_PC_COMBAT_END) == 0);
static_assert(sizeof(sP_CL2FE_REQ_REQUEST_MAKE_BUDDY) == 12 || sizeof(sP_CL2FE_REQ_REQUEST_MAKE_BUDDY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY) == 16 || sizeof(sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE) == 272 || sizeof(sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE) == 272 || sizeof(sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_GET_BUDDY_STYLE) == 12 || sizeof(sP_CL2FE_REQ_GET_BUDDY_STYLE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SET_BUDDY_BLOCK) == 12 || sizeof(sP_CL2FE_REQ_SET_BUDDY_BLOCK) == 0);
static_assert(sizeof(sP_CL2FE_REQ_REMOVE_BUDDY) == 12 || sizeof(sP_CL2FE_REQ_REMOVE_BUDDY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_GET_BUDDY_STATE) == 1 || sizeof(sP_CL2FE_REQ_GET_BUDDY_STATE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_JUMPPAD) == 40 || sizeof(sP_CL2FE_REQ_PC_JUMPPAD) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_LAUNCHER) == 40 || sizeof(sP_CL2FE_REQ_PC_LAUNCHER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ZIPLINE) == 76 || sizeof(sP_CL2FE_REQ_PC_ZIPLINE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM) == 64 || sizeof(sP_CL2FE_REQ_PC_MOVEPLATFORM) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SLOPE) == 48 || sizeof(sP_CL2FE_REQ_PC_SLOPE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_STATE_CHANGE) == 4 || sizeof(sP_CL2FE_REQ_PC_STATE_CHANGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_MAP_WARP) == 4 || sizeof(sP_CL2FE_REQ_PC_MAP_WARP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GIVE_NANO) == 2 || sizeof(sP_CL2FE_REQ_PC_GIVE_NANO) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_SUMMON) == 8 || sizeof(sP_CL2FE_REQ_NPC_SUMMON) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_UNSUMMON) == 4 || sizeof(sP_CL2FE_REQ_NPC_UNSUMMON) == 0);
static_assert(sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN) == 24 || sizeof(sP_CL2FE_REQ_ITEM_CHEST_OPEN) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GIVE_NANO_SKILL) == 4 || sizeof(sP_CL2FE_REQ_PC_GIVE_NANO_SKILL) == 0);
static_assert(sizeof(sP_CL2FE_DOT_DAMAGE_ONOFF) == 4 || sizeof(sP_CL2FE_DOT_DAMAGE_ONOFF) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY) == 28 || sizeof(sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC) == 24 || sizeof(sP_CL2FE_REQ_PC_WARP_USE_NPC) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE) == 4 || sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE) == 4 || sizeof(sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GROUP_JOIN) == 4 || sizeof(sP_CL2FE_REQ_PC_GROUP_JOIN) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_GROUP_LEAVE) == 1 || sizeof(sP_CL2FE_REQ_PC_GROUP_LEAVE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT) == 8 || sizeof(sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_BUDDY_WARP) == 12 || sizeof(sP_CL2FE_REQ_PC_BUDDY_WARP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_GET_MEMBER_STYLE) == 12 || sizeof(sP_CL2FE_REQ_GET_MEMBER_STYLE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_GET_GROUP_STYLE) == 1 || sizeof(sP_CL2FE_REQ_GET_GROUP_STYLE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_CHANGE_MENTOR) == 2 || sizeof(sP_CL2FE_REQ_PC_CHANGE_MENTOR) == 0);
static_assert(sizeof(sP_CL2FE_REQ_GET_BUDDY_LOCATION) == 12 || sizeof(sP_CL2FE_REQ_GET_BUDDY_LOCATION) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_GROUP_SUMMON) == 4 || sizeof(sP_CL2FE_REQ_NPC_GROUP_SUMMON) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_WARP_TO_PC) == 8 || sizeof(sP_CL2FE_REQ_PC_WARP_TO_PC) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RANK_GET_LIST) == 4 || sizeof(sP_CL2FE_REQ_EP_RANK_GET_LIST) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RANK_GET_DETAIL) == 4 || sizeof(sP_CL2FE_REQ_EP_RANK_GET_DETAIL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RANK_GET_PC_INFO) == 56 || sizeof(sP_CL2FE_REQ_EP_RANK_GET_PC_INFO) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RACE_START) == 12 || sizeof(sP_CL2FE_REQ_EP_RACE_START) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RACE_END) == 8 || sizeof(sP_CL2FE_REQ_EP_RACE_END) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_RACE_CANCEL) == 4 || sizeof(sP_CL2FE_REQ_EP_RACE_CANCEL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_EP_GET_RING) == 4 || sizeof(sP_CL2FE_REQ_EP_GET_RING) == 0);
static_assert(sizeof(sP_CL2FE_REQ_IM_CHANGE_SWITCH_STATUS) == 4 || sizeof(sP_CL2FE_REQ_IM_CHANGE_SWITCH_STATUS) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SHINY_PICKUP) == 4 || sizeof(sP_CL2FE_REQ_SHINY_PICKUP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SHINY_SUMMON) == 16 || sizeof(sP_CL2FE_REQ_SHINY_SUMMON) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_MOVETRANSPORTATION) == 60 || sizeof(sP_CL2FE_REQ_PC_MOVETRANSPORTATION) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE) == 260 || sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_ANY_GROUP_FREECHAT_MESSAGE) == 264 || sizeof(sP_CL2FE_REQ_SEND_ANY_GROUP_FREECHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_BARKER) == 8 || sizeof(sP_CL2FE_REQ_BARKER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE) == 260 || sizeof(sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SEND_ANY_GROUP_MENUCHAT_MESSAGE) == 264 || sizeof(sP_CL2FE_REQ_SEND_ANY_GROUP_MENUCHAT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION) == 12 || sizeof(sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION) == 16 || sizeof(sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH) == 8 || sizeof(sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE) == 12 || sizeof(sP_CL2FE_GM_REQ_PC_SET_VALUE) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_KICK_PLAYER) == 76 || sizeof(sP_CL2FE_GM_REQ_KICK_PLAYER) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT) == 172 || sizeof(sP_CL2FE_GM_REQ_TARGET_PC_TELEPORT) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_PC_LOCATION) == 72 || sizeof(sP_CL2FE_GM_REQ_PC_LOCATION) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_PC_ANNOUNCE) == 1032 || sizeof(sP_CL2FE_GM_REQ_PC_ANNOUNCE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_SET_PC_BLOCK) == 12 || sizeof(sP_CL2FE_REQ_SET_PC_BLOCK) == 0);
static_assert(sizeof(sP_CL2FE_REQ_REGIST_RXCOM) == 4 || sizeof(sP_CL2FE_REQ_REGIST_RXCOM) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_PC_MOTD_REGISTER) == 1026 || sizeof(sP_CL2FE_GM_REQ_PC_MOTD_REGISTER) == 0);
static_assert(sizeof(sP_CL2FE_REQ_ITEM_USE) == 12 || sizeof(sP_CL2FE_REQ_ITEM_USE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_WARP_USE_RECALL) == 4 || sizeof(sP_CL2FE_REQ_WARP_USE_RECALL) == 0);
static_assert(sizeof(sP_CL2FE_REP_LIVE_CHECK) == 4 || sizeof(sP_CL2FE_REP_LIVE_CHECK) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_MISSION_COMPLETE) == 4 || sizeof(sP_CL2FE_REQ_PC_MISSION_COMPLETE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TASK_COMPLETE) == 4 || sizeof(sP_CL2FE_REQ_PC_TASK_COMPLETE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_INTERACTION) == 8 || sizeof(sP_CL2FE_REQ_NPC_INTERACTION) == 0);
static_assert(sizeof(sP_CL2FE_DOT_HEAL_ONOFF) == 4 || sizeof(sP_CL2FE_DOT_HEAL_ONOFF) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH) == 8 || sizeof(sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK) == 4 || sizeof(sP_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_READ_EMAIL) == 8 || sizeof(sP_CL2FE_REQ_PC_READ_EMAIL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST) == 1 || sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_DELETE_EMAIL) == 40 || sizeof(sP_CL2FE_REQ_PC_DELETE_EMAIL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SEND_EMAIL) == 1180 || sizeof(sP_CL2FE_REQ_PC_SEND_EMAIL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM) == 16 || sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY) == 8 || sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF) == 80 || sizeof(sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID) == 4 || sizeof(sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_GROUP_INVITE) == 4 || sizeof(sP_CL2FE_REQ_NPC_GROUP_INVITE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_NPC_GROUP_KICK) == 4 || sizeof(sP_CL2FE_REQ_NPC_GROUP_KICK) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_FIRST_USE_FLAG_SET) == 4 || sizeof(sP_CL2FE_REQ_PC_FIRST_USE_FLAG_SET) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TRANSPORT_WARP) == 16 || sizeof(sP_CL2FE_REQ_PC_TRANSPORT_WARP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_TIME_TO_GO_WARP) == 32 || sizeof(sP_CL2FE_REQ_PC_TIME_TO_GO_WARP) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL) == 8 || sizeof(sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_CHANNEL_INFO) == 1 || sizeof(sP_CL2FE_REQ_CHANNEL_INFO) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_CHANNEL_NUM) == 1 || sizeof(sP_CL2FE_REQ_PC_CHANNEL_NUM) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_WARP_CHANNEL) == 8 || sizeof(sP_CL2FE_REQ_PC_WARP_CHANNEL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_LOADING_COMPLETE) == 4 || sizeof(sP_CL2FE_REQ_PC_LOADING_COMPLETE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY) == 52 || sizeof(sP_CL2FE_REQ_PC_FIND_NAME_MAKE_BUDDY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY) == 64 || sizeof(sP_CL2FE_REQ_PC_FIND_NAME_ACCEPT_BUDDY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs) == 4 || sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_READY) == 4 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_READY) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_CANCEL) == 4 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_CANCEL) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_REGIST_ITEM) == 28 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_REGIST_ITEM) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_UNREGIST_ITEM) == 4 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_UNREGIST_ITEM) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_SALE_START) == 4 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_SALE_START) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_ITEM_LIST) == 4 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_ITEM_LIST) == 0);
static_assert(sizeof(sP_CL2FE_PC_STREETSTALL_REQ_ITEM_BUY) == 12 || sizeof(sP_CL2FE_PC_STREETSTALL_REQ_ITEM_BUY) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ITEM_COMBINATION) == 16 || sizeof(sP_CL2FE_REQ_PC_ITEM_COMBINATION) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_SET_PC_SKILL) == 8 || sizeof(sP_CL2FE_GM_REQ_SET_PC_SKILL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SKILL_ADD) == 20 || sizeof(sP_CL2FE_REQ_PC_SKILL_ADD) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SKILL_DEL) == 8 || sizeof(sP_CL2FE_REQ_PC_SKILL_DEL) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_SKILL_USE) == 60 || sizeof(sP_CL2FE_REQ_PC_SKILL_USE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ROPE) == 48 || sizeof(sP_CL2FE_REQ_PC_ROPE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_BELT) == 64 || sizeof(sP_CL2FE_REQ_PC_BELT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VEHICLE_ON) == 1 || sizeof(sP_CL2FE_REQ_PC_VEHICLE_ON) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_VEHICLE_OFF) == 1 || sizeof(sP_CL2FE_REQ_PC_VEHICLE_OFF) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_REGIST_QUICK_SLOT) == 8 || sizeof(sP_CL2FE_REQ_PC_REGIST_QUICK_SLOT) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_DISASSEMBLE_ITEM) == 4 || sizeof(sP_CL2FE_REQ_PC_DISASSEMBLE_ITEM) == 0);
static_assert(sizeof(sP_CL2FE_GM_REQ_REWARD_RATE) == 16 || sizeof(sP_CL2FE_GM_REQ_REWARD_RATE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PC_ITEM_ENCHANT) == 20 || sizeof(sP_CL2FE_REQ_PC_ITEM_ENCHANT) == 0);
static_assert(sizeof(sP_CL2FE_TEXT_MESSAGE) == 256 || sizeof(sP_CL2FE_TEXT_MESSAGE) == 0);
static_assert(sizeof(sP_CL2FE_REQ_PERSIST_CHAR) == 4 || sizeof(sP_CL2FE_REQ_PERSIST_CHAR) == 0);
static_assert(sizeof(sP_CL2FE_REQ_RELOAD_CHAR) == 4 || sizeof(sP_CL2FE_REQ_RELOAD_CHAR) == 0);
static_assert(sizeof(sP_CL2FE_REQ_AI_SCRIPT) == 536 || sizeof(sP_CL2FE_REQ_AI_SCRIPT) == 0);
static_assert(sizeof(sP_CL2LS_REQ_LOGIN) == 468 || sizeof(sP_CL2LS_REQ_LOGIN) == 0);
static_assert(sizeof(sP_CL2LS_REQ_CHECK_CHAR_NAME) == 64 || sizeof(sP_CL2LS_REQ_CHECK_CHAR_NAME) == 0);
static_assert(sizeof(sP_CL2LS_REQ_SAVE_CHAR_NAME) == 68 || sizeof(sP_CL2LS_REQ_SAVE_CHAR_NAME) == 0);
static_assert(sizeof(sP_CL2LS_REQ_CHAR_CREATE) == 100 || sizeof(sP_CL2LS_REQ_CHAR_CREATE) == 0);
static_assert(sizeof(sP_CL2LS_REQ_CHAR_SELECT) == 8 || sizeof(sP_CL2LS_REQ_CHAR_SELECT) == 0);
static_assert(sizeof(sP_CL2LS_REQ_CHAR_DELETE) == 8 || sizeof(sP_CL2LS_REQ_CHAR_DELETE) == 0);
static_assert(sizeof(sP_CL2LS_REQ_SHARD_SELECT) == 1 || sizeof(sP_CL2LS_REQ_SHARD_SELECT) == 0);
static_assert(sizeof(sP_CL2LS_REQ_SHARD_LIST_INFO) == 1 || sizeof(sP_CL2LS_REQ_SHARD_LIST_INFO) == 0);
static_assert(sizeof(sP_CL2LS_CHECK_NAME_LIST) == 12 || sizeof(sP_CL2LS_CHECK_NAME_LIST) == 0);
static_assert(sizeof(sP_CL2LS_REQ_SAVE_CHAR_TUTOR) == 12 || sizeof(sP_CL2LS_REQ_SAVE_CHAR_TUTOR) == 0);
static_assert(sizeof(sP_CL2LS_REQ_PC_EXIT_DUPLICATE) == 132 || sizeof(sP_CL2LS_REQ_PC_EXIT_DUPLICATE) == 0);
static_assert(sizeof(sP_CL2LS_REP_LIVE_CHECK) == 4 || sizeof(sP_CL2LS_REP_LIVE_CHECK) == 0);
static_assert(sizeof(sP_CL2LS_REQ_CHANGE_CHAR_NAME) == 76 || sizeof(sP_CL2LS_REQ_CHANGE_CHAR_NAME) == 0);
static_assert(sizeof(sP_CL2LS_REQ_SERVER_SELECT) == 1 || sizeof(sP_CL2LS_REQ_SERVER_SELECT) == 0);
static_assert(sizeof(sPacket) == 4100 || sizeof(sPacket) == 0);
static_assert(sizeof(sPacket_Full) == 4104 || sizeof(sPacket_Full) == 0);
static_assert(sizeof(sPacket2x) == 8196 || sizeof(sPacket2x) == 0);
static_assert(sizeof(sPacket2x_Full) == 8200 || sizeof(sPacket2x_Full) == 0);
static_assert(sizeof(sP_FE2CL_ERROR) == 4 || sizeof(sP_FE2CL_ERROR) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ENTER_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_ENTER_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ENTER_SUCC) == 3184 || sizeof(sP_FE2CL_REP_PC_ENTER_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_NEW) == 316 || sizeof(sP_FE2CL_PC_NEW) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_EXIT_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_EXIT_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_EXIT_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_EXIT_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_EXIT) == 8 || sizeof(sP_FE2CL_PC_EXIT) == 0);
static_assert(sizeof(sP_FE2CL_PC_AROUND) == 4 || sizeof(sP_FE2CL_PC_AROUND) == 0);
static_assert(sizeof(sP_FE2CL_PC_MOVE) == 56 || sizeof(sP_FE2CL_PC_MOVE) == 0);
static_assert(sizeof(sP_FE2CL_PC_STOP) == 32 || sizeof(sP_FE2CL_PC_STOP) == 0);
static_assert(sizeof(sP_FE2CL_PC_JUMP) == 56 || sizeof(sP_FE2CL_PC_JUMP) == 0);
static_assert(sizeof(sP_FE2CL_NPC_ENTER) == 36 || sizeof(sP_FE2CL_NPC_ENTER) == 0);
static_assert(sizeof(sP_FE2CL_NPC_EXIT) == 4 || sizeof(sP_FE2CL_NPC_EXIT) == 0);
static_assert(sizeof(sP_FE2CL_NPC_MOVE) == 24 || sizeof(sP_FE2CL_NPC_MOVE) == 0);
static_assert(sizeof(sP_FE2CL_NPC_NEW) == 36 || sizeof(sP_FE2CL_NPC_NEW) == 0);
static_assert(sizeof(sP_FE2CL_NPC_AROUND) == 4 || sizeof(sP_FE2CL_NPC_AROUND) == 0);
static_assert(sizeof(sP_FE2CL_AROUND_DEL_PC) == 4 || sizeof(sP_FE2CL_AROUND_DEL_PC) == 0);
static_assert(sizeof(sP_FE2CL_AROUND_DEL_NPC) == 4 || sizeof(sP_FE2CL_AROUND_DEL_NPC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC) == 264 || sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_FAIL) == 264 || sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == 8 || sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs) == 8 || sizeof(sP_FE2CL_PC_ATTACK_NPCs) == 0);
static_assert(sizeof(sP_FE2CL_NPC_ATTACK_PCs) == 8 || sizeof(sP_FE2CL_NPC_ATTACK_PCs) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_REGEN_SUCC) == 48 || sizeof(sP_FE2CL_REP_PC_REGEN_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC) == 264 || sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_FAIL) == 264 || sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC) == 48 || sizeof(sP_FE2CL_PC_ITEM_MOVE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_EQUIP_CHANGE) == 24 || sizeof(sP_FE2CL_PC_EQUIP_CHANGE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_START_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_TASK_START_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_TASK_END_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_END_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_TASK_END_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_READY) == 20 || sizeof(sP_FE2CL_NPC_SKILL_READY) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_FIRE) == 20 || sizeof(sP_FE2CL_NPC_SKILL_FIRE) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_HIT) == 28 || sizeof(sP_FE2CL_NPC_SKILL_HIT) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_READY) == 20 || sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_READY) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT) == 24 || sizeof(sP_FE2CL_NPC_SKILL_CORRUPTION_HIT) == 0);
static_assert(sizeof(sP_FE2CL_NPC_SKILL_CANCEL) == 4 || sizeof(sP_FE2CL_NPC_SKILL_CANCEL) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC) == 8 || sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_UNEQUIP_SUCC) == 8 || sizeof(sP_FE2CL_REP_NANO_UNEQUIP_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC) == 8 || sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC) == 208 || sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_NANO_ACTIVE) == 20 || sizeof(sP_FE2CL_NANO_ACTIVE) == 0);
static_assert(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) == 36 || sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_NANO_SKILL_USE) == 36 || sizeof(sP_FE2CL_NANO_SKILL_USE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_STOP_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_TASK_STOP_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_STOP_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_TASK_STOP_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_CONTINUE_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_TASK_CONTINUE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TASK_CONTINUE_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_TASK_CONTINUE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_GOTO_SUCC) == 12 || sizeof(sP_FE2CL_REP_PC_GOTO_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_CHARGE_NANO_STAMINA) == 8 || sizeof(sP_FE2CL_REP_CHARGE_NANO_STAMINA) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TICK) == 32 || sizeof(sP_FE2CL_REP_PC_TICK) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC) == 24 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC) == 40 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_ITEM_DELETE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ROCKET_STYLE_READY) == 8 || sizeof(sP_FE2CL_PC_ROCKET_STYLE_READY) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ROCKET_STYLE_FIRE_SUCC) == 56 || sizeof(sP_FE2CL_REP_PC_ROCKET_STYLE_FIRE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ROCKET_STYLE_FIRE) == 48 || sizeof(sP_FE2CL_PC_ROCKET_STYLE_FIRE) == 0);
static_assert(sizeof(sP_FE2CL_PC_ROCKET_STYLE_HIT) == 24 || sizeof(sP_FE2CL_PC_ROCKET_STYLE_HIT) == 0);
static_assert(sizeof(sP_FE2CL_PC_GRENADE_STYLE_READY) == 8 || sizeof(sP_FE2CL_PC_GRENADE_STYLE_READY) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC) == 44 || sizeof(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_GRENADE_STYLE_FIRE) == 36 || sizeof(sP_FE2CL_PC_GRENADE_STYLE_FIRE) == 0);
static_assert(sizeof(sP_FE2CL_PC_GRENADE_STYLE_HIT) == 24 || sizeof(sP_FE2CL_PC_GRENADE_STYLE_HIT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_OFFER) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_OFFER) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_CANCEL) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_CANCEL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_ABORT) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_ABORT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT) == 12 || sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC) == 400 || sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_FAIL) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC) == 44 || sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_FAIL) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC) == 44 || sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_FAIL) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_FAIL) == 16 || sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT) == 272 || sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC) == 32 || sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_NANO_CREATE_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_NANO_CREATE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_TUNE_FAIL) == 8 || sizeof(sP_FE2CL_REP_NANO_TUNE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BANK_OPEN_SUCC) == 3204 || sizeof(sP_FE2CL_REP_PC_BANK_OPEN_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BANK_OPEN_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_BANK_OPEN_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BANK_CLOSE_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_BANK_CLOSE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BANK_CLOSE_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_BANK_CLOSE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_START_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_VENDOR_START_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_START_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_START_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC) == 560 || sizeof(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC) == 24 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT) == 12 || sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC) == 24 || sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC) == 16 || sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_BUDDYLIST_INFO_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC) == 16 || sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL) == 16 || sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC) == 76 || sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL) == 16 || sizeof(sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC) == 276 || sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_FAIL) == 272 || sizeof(sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC) == 276 || sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_FAIL) == 272 || sizeof(sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_STYLE_SUCC) == 280 || sizeof(sP_FE2CL_REP_GET_BUDDY_STYLE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_STYLE_FAIL) == 12 || sizeof(sP_FE2CL_REP_GET_BUDDY_STYLE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC) == 252 || sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_FAIL) == 4 || sizeof(sP_FE2CL_REP_GET_BUDDY_STATE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC) == 12 || sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_FAIL) == 12 || sizeof(sP_FE2CL_REP_SET_BUDDY_BLOCK_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_REMOVE_BUDDY_SUCC) == 12 || sizeof(sP_FE2CL_REP_REMOVE_BUDDY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_REMOVE_BUDDY_FAIL) == 12 || sizeof(sP_FE2CL_REP_REMOVE_BUDDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_JUMPPAD) == 52 || sizeof(sP_FE2CL_PC_JUMPPAD) == 0);
static_assert(sizeof(sP_FE2CL_PC_LAUNCHER) == 52 || sizeof(sP_FE2CL_PC_LAUNCHER) == 0);
static_assert(sizeof(sP_FE2CL_PC_ZIPLINE) == 88 || sizeof(sP_FE2CL_PC_ZIPLINE) == 0);
static_assert(sizeof(sP_FE2CL_PC_MOVEPLATFORM) == 76 || sizeof(sP_FE2CL_PC_MOVEPLATFORM) == 0);
static_assert(sizeof(sP_FE2CL_PC_SLOPE) == 60 || sizeof(sP_FE2CL_PC_SLOPE) == 0);
static_assert(sizeof(sP_FE2CL_PC_STATE_CHANGE) == 8 || sizeof(sP_FE2CL_PC_STATE_CHANGE) == 0);
static_assert(sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER) == 60 || sizeof(sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER) == 0);
static_assert(sizeof(sP_FE2CL_REP_REWARD_ITEM) == 36 || sizeof(sP_FE2CL_REP_REWARD_ITEM) == 0);
static_assert(sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC) == 4 || sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_FAIL) == 8 || sizeof(sP_FE2CL_REP_ITEM_CHEST_OPEN_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) == 12 || sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC) == 12 || sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_NPC_ROCKET_STYLE_FIRE) == 48 || sizeof(sP_FE2CL_NPC_ROCKET_STYLE_FIRE) == 0);
static_assert(sizeof(sP_FE2CL_NPC_GRENADE_STYLE_FIRE) == 36 || sizeof(sP_FE2CL_NPC_GRENADE_STYLE_FIRE) == 0);
static_assert(sizeof(sP_FE2CL_NPC_BULLET_STYLE_HIT) == 28 || sizeof(sP_FE2CL_NPC_BULLET_STYLE_HIT) == 0);
static_assert(sizeof(sP_FE2CL_CHARACTER_ATTACK_CHARACTERs) == 12 || sizeof(sP_FE2CL_CHARACTER_ATTACK_CHARACTERs) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_INVITE) == 4 || sizeof(sP_FE2CL_PC_GROUP_INVITE) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_INVITE_FAIL) == 4 || sizeof(sP_FE2CL_PC_GROUP_INVITE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_INVITE_REFUSE) == 4 || sizeof(sP_FE2CL_PC_GROUP_INVITE_REFUSE) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_JOIN) == 12 || sizeof(sP_FE2CL_PC_GROUP_JOIN) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_JOIN_FAIL) == 8 || sizeof(sP_FE2CL_PC_GROUP_JOIN_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_JOIN_SUCC) == 12 || sizeof(sP_FE2CL_PC_GROUP_JOIN_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_LEAVE) == 12 || sizeof(sP_FE2CL_PC_GROUP_LEAVE) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_LEAVE_FAIL) == 8 || sizeof(sP_FE2CL_PC_GROUP_LEAVE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC) == 1 || sizeof(sP_FE2CL_PC_GROUP_LEAVE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) == 12 || sizeof(sP_FE2CL_PC_GROUP_MEMBER_INFO) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC) == 40 || sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_WARP_USE_NPC_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT) == 8 || sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_CHANGE_MENTOR_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_CHANGE_MENTOR_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_MEMBER_STYLE_FAIL) == 4 || sizeof(sP_FE2CL_REP_GET_MEMBER_STYLE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_MEMBER_STYLE_SUCC) == 280 || sizeof(sP_FE2CL_REP_GET_MEMBER_STYLE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_GROUP_STYLE_FAIL) == 4 || sizeof(sP_FE2CL_REP_GET_GROUP_STYLE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_GROUP_STYLE_SUCC) == 4 || sizeof(sP_FE2CL_REP_GET_GROUP_STYLE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_REGEN) == 36 || sizeof(sP_FE2CL_PC_REGEN) == 0);
static_assert(sizeof(sP_FE2CL_INSTANCE_MAP_INFO) == 60 || sizeof(sP_FE2CL_INSTANCE_MAP_INFO) == 0);
static_assert(sizeof(sP_FE2CL_TRANSPORTATION_ENTER) == 24 || sizeof(sP_FE2CL_TRANSPORTATION_ENTER) == 0);
static_assert(sizeof(sP_FE2CL_TRANSPORTATION_EXIT) == 8 || sizeof(sP_FE2CL_TRANSPORTATION_EXIT) == 0);
static_assert(sizeof(sP_FE2CL_TRANSPORTATION_MOVE) == 28 || sizeof(sP_FE2CL_TRANSPORTATION_MOVE) == 0);
static_assert(sizeof(sP_FE2CL_TRANSPORTATION_NEW) == 24 || sizeof(sP_FE2CL_TRANSPORTATION_NEW) == 0);
static_assert(sizeof(sP_FE2CL_TRANSPORTATION_AROUND) == 4 || sizeof(sP_FE2CL_TRANSPORTATION_AROUND) == 0);
static_assert(sizeof(sP_FE2CL_AROUND_DEL_TRANSPORTATION) == 8 || sizeof(sP_FE2CL_AROUND_DEL_TRANSPORTATION) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RANK_LIST) == 1 || sizeof(sP_FE2CL_REP_EP_RANK_LIST) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RANK_DETAIL) == 1 || sizeof(sP_FE2CL_REP_EP_RANK_DETAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RANK_PC_INFO) == 1 || sizeof(sP_FE2CL_REP_EP_RANK_PC_INFO) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_START_SUCC) == 12 || sizeof(sP_FE2CL_REP_EP_RACE_START_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_START_FAIL) == 4 || sizeof(sP_FE2CL_REP_EP_RACE_START_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_END_SUCC) == 76 || sizeof(sP_FE2CL_REP_EP_RACE_END_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_END_FAIL) == 4 || sizeof(sP_FE2CL_REP_EP_RACE_END_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_CANCEL_SUCC) == 4 || sizeof(sP_FE2CL_REP_EP_RACE_CANCEL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_RACE_CANCEL_FAIL) == 4 || sizeof(sP_FE2CL_REP_EP_RACE_CANCEL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_GET_RING_SUCC) == 8 || sizeof(sP_FE2CL_REP_EP_GET_RING_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_EP_GET_RING_FAIL) == 4 || sizeof(sP_FE2CL_REP_EP_GET_RING_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_IM_CHANGE_SWITCH_STATUS) == 16 || sizeof(sP_FE2CL_REP_IM_CHANGE_SWITCH_STATUS) == 0);
static_assert(sizeof(sP_FE2CL_SHINY_ENTER) == 24 || sizeof(sP_FE2CL_SHINY_ENTER) == 0);
static_assert(sizeof(sP_FE2CL_SHINY_EXIT) == 4 || sizeof(sP_FE2CL_SHINY_EXIT) == 0);
static_assert(sizeof(sP_FE2CL_SHINY_NEW) == 24 || sizeof(sP_FE2CL_SHINY_NEW) == 0);
static_assert(sizeof(sP_FE2CL_SHINY_AROUND) == 4 || sizeof(sP_FE2CL_SHINY_AROUND) == 0);
static_assert(sizeof(sP_FE2CL_AROUND_DEL_SHINY) == 4 || sizeof(sP_FE2CL_AROUND_DEL_SHINY) == 0);
static_assert(sizeof(sP_FE2CL_REP_SHINY_PICKUP_FAIL) == 1 || sizeof(sP_FE2CL_REP_SHINY_PICKUP_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SHINY_PICKUP_SUCC) == 8 || sizeof(sP_FE2CL_REP_SHINY_PICKUP_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_MOVETRANSPORTATION) == 72 || sizeof(sP_FE2CL_PC_MOVETRANSPORTATION) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC) == 264 || sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_FAIL) == 268 || sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_SUCC) == 268 || sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_FAIL) == 272 || sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_BARKER) == 8 || sizeof(sP_FE2CL_REP_BARKER) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC) == 264 || sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_FAIL) == 268 || sizeof(sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_SUCC) == 268 || sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_FAIL) == 272 || sizeof(sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC) == 28 || sizeof(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC) == 20 || sizeof(sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_ANNOUNCE_MSG) == 1032 || sizeof(sP_FE2CL_ANNOUNCE_MSG) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_SPECIAL_STATE_CHANGE) == 8 || sizeof(sP_FE2CL_PC_SPECIAL_STATE_CHANGE) == 0);
static_assert(sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE) == 12 || sizeof(sP_FE2CL_GM_REP_PC_SET_VALUE) == 0);
static_assert(sizeof(sP_FE2CL_GM_PC_CHANGE_VALUE) == 12 || sizeof(sP_FE2CL_GM_PC_CHANGE_VALUE) == 0);
static_assert(sizeof(sP_FE2CL_GM_REP_PC_LOCATION) == 96 || sizeof(sP_FE2CL_GM_REP_PC_LOCATION) == 0);
static_assert(sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE) == 1032 || sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL) == 8 || sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL) == 0);
static_assert(sizeof(sP_FE2CL_REP_SET_PC_BLOCK_SUCC) == 16 || sizeof(sP_FE2CL_REP_SET_PC_BLOCK_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_SET_PC_BLOCK_FAIL) == 16 || sizeof(sP_FE2CL_REP_SET_PC_BLOCK_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_REGIST_RXCOM) == 16 || sizeof(sP_FE2CL_REP_REGIST_RXCOM) == 0);
static_assert(sizeof(sP_FE2CL_REP_REGIST_RXCOM_FAIL) == 4 || sizeof(sP_FE2CL_REP_REGIST_RXCOM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_INVEN_FULL_MSG) == 8 || sizeof(sP_FE2CL_PC_INVEN_FULL_MSG) == 0);
static_assert(sizeof(sP_FE2CL_REQ_LIVE_CHECK) == 4 || sizeof(sP_FE2CL_REQ_LIVE_CHECK) == 0);
static_assert(sizeof(sP_FE2CL_PC_MOTD_LOGIN) == 1026 || sizeof(sP_FE2CL_PC_MOTD_LOGIN) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_ITEM_USE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC) == 40 || sizeof(sP_FE2CL_REP_PC_ITEM_USE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ITEM_USE) == 16 || sizeof(sP_FE2CL_PC_ITEM_USE) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_LOCATION_SUCC) == 28 || sizeof(sP_FE2CL_REP_GET_BUDDY_LOCATION_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_GET_BUDDY_LOCATION_FAIL) == 16 || sizeof(sP_FE2CL_REP_GET_BUDDY_LOCATION_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RIDING_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_RIDING_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RIDING_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_RIDING_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_RIDING) == 8 || sizeof(sP_FE2CL_PC_RIDING) == 0);
static_assert(sizeof(sP_FE2CL_PC_BROOMSTICK_MOVE) == 20 || sizeof(sP_FE2CL_PC_BROOMSTICK_MOVE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_OTHER_SHARD_SUCC) == 16 || sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_OTHER_SHARD_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL) == 4 || sizeof(sP_FE2CL_REP_WARP_USE_RECALL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_EXIT_DUPLICATE) == 4 || sizeof(sP_FE2CL_REP_PC_EXIT_DUPLICATE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_MISSION_COMPLETE_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_MISSION_COMPLETE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_BUFF_UPDATE) == 44 || sizeof(sP_FE2CL_PC_BUFF_UPDATE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_NEW_EMAIL) == 4 || sizeof(sP_FE2CL_REP_PC_NEW_EMAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_READ_EMAIL_SUCC) == 1100 || sizeof(sP_FE2CL_REP_PC_READ_EMAIL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_READ_EMAIL_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_READ_EMAIL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC) == 1024 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC) == 40 || sizeof(sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_DELETE_EMAIL_FAIL) == 44 || sizeof(sP_FE2CL_REP_PC_DELETE_EMAIL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SEND_EMAIL_SUCC) == 92 || sizeof(sP_FE2CL_REP_PC_SEND_EMAIL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SEND_EMAIL_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_SEND_EMAIL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC) == 16 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_FAIL) == 20 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC) == 12 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_SUDDEN_DEAD) == 16 || sizeof(sP_FE2CL_PC_SUDDEN_DEAD) == 0);
static_assert(sizeof(sP_FE2CL_REP_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF_SUCC) == 64 || sizeof(sP_FE2CL_REP_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID) == 4 || sizeof(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID) == 0);
static_assert(sizeof(sP_FE2CL_REP_NPC_GROUP_INVITE_FAIL) == 4 || sizeof(sP_FE2CL_REP_NPC_GROUP_INVITE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_NPC_GROUP_INVITE_SUCC) == 16 || sizeof(sP_FE2CL_REP_NPC_GROUP_INVITE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_NPC_GROUP_KICK_FAIL) == 4 || sizeof(sP_FE2CL_REP_NPC_GROUP_KICK_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_NPC_GROUP_KICK_SUCC) == 16 || sizeof(sP_FE2CL_REP_NPC_GROUP_KICK_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_EVENT) == 20 || sizeof(sP_FE2CL_PC_EVENT) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRANSPORT_WARP_SUCC) == 36 || sizeof(sP_FE2CL_REP_PC_TRANSPORT_WARP_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT_FAIL) == 276 || sizeof(sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL) == 12 || sizeof(sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC) == 4 || sizeof(sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_CHANNEL_INFO) == 8 || sizeof(sP_FE2CL_REP_CHANNEL_INFO) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_CHANNEL_NUM) == 4 || sizeof(sP_FE2CL_REP_PC_CHANNEL_NUM) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_CHANNEL_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_WARP_CHANNEL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_WARP_CHANNEL_SUCC) == 1 || sizeof(sP_FE2CL_REP_PC_WARP_CHANNEL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC) == 64 || sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_FAIL) == 56 || sizeof(sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_FIND_NAME_ACCEPT_BUDDY_FAIL) == 68 || sizeof(sP_FE2CL_REP_PC_FIND_NAME_ACCEPT_BUDDY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_SAME_SHARD_SUCC) == 1 || sizeof(sP_FE2CL_REP_PC_BUDDY_WARP_SAME_SHARD_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) == 8 || sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_ATTACK_CHARs) == 8 || sizeof(sP_FE2CL_PC_ATTACK_CHARs) == 0);
static_assert(sizeof(sP_FE2CL_NPC_ATTACK_CHARs) == 8 || sizeof(sP_FE2CL_NPC_ATTACK_CHARs) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_NANO_CREATE) == 8 || sizeof(sP_FE2CL_REP_PC_NANO_CREATE) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_READY_SUCC) == 16 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_READY_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_READY_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_READY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_CANCEL_SUCC) == 1 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_CANCEL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_CANCEL_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_CANCEL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_SUCC) == 28 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_SUCC) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_SALE_START_SUCC) == 24 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_SALE_START_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_SALE_START_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_SALE_START_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST) == 8 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_BUYER) == 32 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_BUYER) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_SELLER) == 32 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_SELLER) == 0);
static_assert(sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_FAIL) == 4 || sizeof(sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC) == 40 || sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL) == 20 || sizeof(sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_CASH_BUFF_UPDATE) == 40 || sizeof(sP_FE2CL_PC_CASH_BUFF_UPDATE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_ADD_SUCC) == 28 || sizeof(sP_FE2CL_REP_PC_SKILL_ADD_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_ADD_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_SKILL_ADD_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_DEL_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_SKILL_DEL_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_DEL_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_SKILL_DEL_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_USE_SUCC) == 60 || sizeof(sP_FE2CL_REP_PC_SKILL_USE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_SKILL_USE_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_SKILL_USE_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_SKILL_USE) == 60 || sizeof(sP_FE2CL_PC_SKILL_USE) == 0);
static_assert(sizeof(sP_FE2CL_PC_ROPE) == 60 || sizeof(sP_FE2CL_PC_ROPE) == 0);
static_assert(sizeof(sP_FE2CL_PC_BELT) == 76 || sizeof(sP_FE2CL_PC_BELT) == 0);
static_assert(sizeof(sP_FE2CL_PC_VEHICLE_ON_SUCC) == 1 || sizeof(sP_FE2CL_PC_VEHICLE_ON_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_VEHICLE_ON_FAIL) == 4 || sizeof(sP_FE2CL_PC_VEHICLE_ON_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC) == 1 || sizeof(sP_FE2CL_PC_VEHICLE_OFF_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_VEHICLE_OFF_FAIL) == 4 || sizeof(sP_FE2CL_PC_VEHICLE_OFF_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_PC_QUICK_SLOT_INFO) == 32 || sizeof(sP_FE2CL_PC_QUICK_SLOT_INFO) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_FAIL) == 4 || sizeof(sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_SUCC) == 8 || sizeof(sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM) == 4 || sizeof(sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_SUCC) == 20 || sizeof(sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_FAIL) == 8 || sizeof(sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_GM_REP_REWARD_RATE_SUCC) == 40 || sizeof(sP_FE2CL_GM_REP_REWARD_RATE_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_ENCHANT_SUCC) == 76 || sizeof(sP_FE2CL_REP_PC_ITEM_ENCHANT_SUCC) == 0);
static_assert(sizeof(sP_FE2CL_REP_PC_ITEM_ENCHANT_FAIL) == 24 || sizeof(sP_FE2CL_REP_PC_ITEM_ENCHANT_FAIL) == 0);
static_assert(sizeof(sP_FE2CL_TEXT_MESSAGE) == 256 || sizeof(sP_FE2CL_TEXT_MESSAGE) == 0);
static_assert(sizeof(sP_FE2CL_REP_PERSIST_CHAR) == 8 || sizeof(sP_FE2CL_REP_PERSIST_CHAR) == 0);
static_assert(sizeof(sP_FE2CL_REP_RELOAD_CHAR) == 5272 || sizeof(sP_FE2CL_REP_RELOAD_CHAR) == 0);
static_assert(sizeof(sP_FE2CL_REP_NANO_BOOK_SUBSET) == 76 || sizeof(sP_FE2CL_REP_NANO_BOOK_SUBSET) == 0);
static_assert(sizeof(sP_LS2CL_REP_LOGIN_SUCC) == 88 || sizeof(sP_LS2CL_REP_LOGIN_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_LOGIN_FAIL) == 72 || sizeof(sP_LS2CL_REP_LOGIN_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_INFO) == 288 || sizeof(sP_LS2CL_REP_CHAR_INFO) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC) == 52 || sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL) == 4 || sizeof(sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC) == 64 || sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_FAIL) == 4 || sizeof(sP_LS2CL_REP_SAVE_CHAR_NAME_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC) == 100 || sizeof(sP_LS2CL_REP_CHAR_CREATE_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_CREATE_FAIL) == 4 || sizeof(sP_LS2CL_REP_CHAR_CREATE_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC) == 1 || sizeof(sP_LS2CL_REP_CHAR_SELECT_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_SELECT_FAIL) == 4 || sizeof(sP_LS2CL_REP_CHAR_SELECT_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_DELETE_SUCC) == 1 || sizeof(sP_LS2CL_REP_CHAR_DELETE_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHAR_DELETE_FAIL) == 4 || sizeof(sP_LS2CL_REP_CHAR_DELETE_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_SHARD_SELECT_SUCC) == 28 || sizeof(sP_LS2CL_REP_SHARD_SELECT_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_SHARD_SELECT_FAIL) == 4 || sizeof(sP_LS2CL_REP_SHARD_SELECT_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_VERSION_CHECK_SUCC) == 1 || sizeof(sP_LS2CL_REP_VERSION_CHECK_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_VERSION_CHECK_FAIL) == 4 || sizeof(sP_LS2CL_REP_VERSION_CHECK_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHECK_NAME_LIST_SUCC) == 76 || sizeof(sP_LS2CL_REP_CHECK_NAME_LIST_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHECK_NAME_LIST_FAIL) == 16 || sizeof(sP_LS2CL_REP_CHECK_NAME_LIST_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_PC_EXIT_DUPLICATE) == 4 || sizeof(sP_LS2CL_REP_PC_EXIT_DUPLICATE) == 0);
static_assert(sizeof(sP_LS2CL_REQ_LIVE_CHECK) == 4 || sizeof(sP_LS2CL_REQ_LIVE_CHECK) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC) == 64 || sizeof(sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC) == 0);
static_assert(sizeof(sP_LS2CL_REP_CHANGE_CHAR_NAME_FAIL) == 16 || sizeof(sP_LS2CL_REP_CHANGE_CHAR_NAME_FAIL) == 0);
static_assert(sizeof(sP_LS2CL_REP_SHARD_LIST_INFO_SUCC) == 26 || sizeof(sP_LS2CL_REP_SHARD_LIST_INFO_SUCC) == 0);
static_assert(sizeof(sPCStyle) == 76 || sizeof(sPCStyle) == 0);
static_assert(sizeof(sOnItem) == 14 || sizeof(sOnItem) == 0);
static_assert(sizeof(sOnItem_Index) == 10 || sizeof(sOnItem_Index) == 0);
static_assert(sizeof(sPCStyle2) == 3 || sizeof(sPCStyle2) == 0);
static_assert(sizeof(sItemBase) == 16 || sizeof(sItemBase) == 0);
static_assert(sizeof(sItemTrade) == 16 || sizeof(sItemTrade) == 0);
static_assert(sizeof(sEmailItemInfoFromCL) == 20 || sizeof(sEmailItemInfoFromCL) == 0);
static_assert(sizeof(sPCLoadData2CL) == 3172 || sizeof(sPCLoadData2CL) == 0);
static_assert(sizeof(sPCAppearanceData) == 316 || sizeof(sPCAppearanceData) == 0);
static_assert(sizeof(sNPCAppearanceData) == 36 || sizeof(sNPCAppearanceData) == 0);
static_assert(sizeof(sPCRegenData) == 40 || sizeof(sPCRegenData) == 0);
static_assert(sizeof(sNano) == 6 || sizeof(sNano) == 0);
static_assert(sizeof(sPCBullet) == 12 || sizeof(sPCBullet) == 0);
static_assert(sizeof(sBuddyBaseInfo) == 72 || sizeof(sBuddyBaseInfo) == 0);
static_assert(sizeof(sBuddyStyleInfo) == 268 || sizeof(sBuddyStyleInfo) == 0);
static_assert(sizeof(sNPCBullet) == 16 || sizeof(sNPCBullet) == 0);
static_assert(sizeof(sPCRegenDataForOtherPC) == 36 || sizeof(sPCRegenDataForOtherPC) == 0);
static_assert(sizeof(sTransportationAppearanceData) == 24 || sizeof(sTransportationAppearanceData) == 0);
static_assert(sizeof(sItemReward) == 24 || sizeof(sItemReward) == 0);
static_assert(sizeof(sShinyAppearanceData) == 24 || sizeof(sShinyAppearanceData) == 0);
static_assert(sizeof(sTimeBuff) == 28 || sizeof(sTimeBuff) == 0);
static_assert(sizeof(sItemVendor) == 28 || sizeof(sItemVendor) == 0);
static_assert(sizeof(sEmailInfo) == 204 || sizeof(sEmailInfo) == 0);
static_assert(sizeof(sQuickSlot) == 4 || sizeof(sQuickSlot) == 0);
static_assert(sizeof(sItemInven) == 16 || sizeof(sItemInven) == 0);
static_assert(sizeof(sRunningQuest) == 52 || sizeof(sRunningQuest) == 0);
static_assert(sizeof(sSYSTEMTIME) == 32 || sizeof(sSYSTEMTIME) == 0);
static_assert(sizeof(sTimeLimitItemDeleteInfo2CL) == 8 || sizeof(sTimeLimitItemDeleteInfo2CL) == 0);
static_assert(sizeof(sEPRecord) == 6 || sizeof(sEPRecord) == 0);
static_assert(sizeof(sNanoBank) == 4 || sizeof(sNanoBank) == 0);
static_assert(sizeof(sTimeBuff_Svr) == 32 || sizeof(sTimeBuff_Svr) == 0);
static_assert(sizeof(sBulletAppearanceData) == 20 || sizeof(sBulletAppearanceData) == 0);
static_assert(sizeof(sTransportationLoadData) == 32 || sizeof(sTransportationLoadData) == 0);
static_assert(sizeof(sAttackResult) == 24 || sizeof(sAttackResult) == 0);
static_assert(sizeof(sCAttackResult) == 40 || sizeof(sCAttackResult) == 0);
static_assert(sizeof(sSkillResult_Damage) == 20 || sizeof(sSkillResult_Damage) == 0);
static_assert(sizeof(sSkillResult_DotDamage) == 32 || sizeof(sSkillResult_DotDamage) == 0);
static_assert(sizeof(sSkillResult_Heal_HP) == 16 || sizeof(sSkillResult_Heal_HP) == 0);
static_assert(sizeof(sSkillResult_Heal_Stamina) == 16 || sizeof(sSkillResult_Heal_Stamina) == 0);
static_assert(sizeof(sSkillResult_Stamina_Self) == 24 || sizeof(sSkillResult_Stamina_Self) == 0);
static_assert(sizeof(sSkillResult_Damage_N_Debuff) == 32 || sizeof(sSkillResult_Damage_N_Debuff) == 0);
static_assert(sizeof(sSkillResult_Buff) == 16 || sizeof(sSkillResult_Buff) == 0);
static_assert(sizeof(sSkillResult_BatteryDrain) == 40 || sizeof(sSkillResult_BatteryDrain) == 0);
static_assert(sizeof(sSkillResult_Damage_N_Move) == 36 || sizeof(sSkillResult_Damage_N_Move) == 0);
static_assert(sizeof(sSkillResult_Move) == 24 || sizeof(sSkillResult_Move) == 0);
static_assert(sizeof(sSkillResult_Resurrect) == 12 || sizeof(sSkillResult_Resurrect) == 0);
static_assert(sizeof(sPC_HP) == 8 || sizeof(sPC_HP) == 0);
static_assert(sizeof(sPC_BATTERYs) == 12 || sizeof(sPC_BATTERYs) == 0);
static_assert(sizeof(sPC_NanoSlots) == 16 || sizeof(sPC_NanoSlots) == 0);
static_assert(sizeof(sPC_Nano) == 12 || sizeof(sPC_Nano) == 0);
static_assert(sizeof(sNPCLocationData) == 24 || sizeof(sNPCLocationData) == 0);
static_assert(sizeof(sGroupNPCLocationData) == 44 || sizeof(sGroupNPCLocationData) == 0);
static_assert(sizeof(sPCGroupMemberInfo) == 112 || sizeof(sPCGroupMemberInfo) == 0);
static_assert(sizeof(sNPCGroupMemberInfo) == 32 || sizeof(sNPCGroupMemberInfo) == 0);
static_assert(sizeof(sEPElement) == 36 || sizeof(sEPElement) == 0);
static_assert(sizeof(sChannelInfo) == 8 || sizeof(sChannelInfo) == 0);
