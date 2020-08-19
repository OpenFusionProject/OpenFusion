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

#pragma pack(1)
struct sPCStyle2 {
	int8_t iAppearanceFlag;
	int8_t iTutorialFlag;
	int8_t iPayzoneFlag;
};

#pragma pack(4)
struct sRunningQuest {
	int32_t m_aCurrTaskID;
	int m_aKillNPCID[3];
	int m_aKillNPCCount[3];
	int m_aNeededItemID[3];
	int m_aNeededItemCount[3];
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

#pragma pack(4)
struct sItemBase {
	int16_t iType;
	int16_t iID;
	int32_t iOpt;
	int32_t iTimeLimit;
};

struct sItemTrade {
	int16_t iType;
	int16_t iID;
	int32_t iOpt;
	int32_t iInvenNum;
	int32_t iSlotNum;
};

struct sItemVendor {
	int32_t iVendorID;
	float fBuyCost;
	sItemBase item;
	int32_t iSortNum;
};

struct sItemReward {
	sItemBase sItem;
	int32_t eIL;
	int32_t iSlotNum;
};

struct sTimeLimitItemDeleteInfo2CL {
	int32_t eIL;
	int32_t iSlotNum;
};

struct sNanoTuneNeedItemInfo2CL {
	int32_t iSlotNum;
	sItemBase ItemBase;
};

struct sEmailItemInfoFromCL {
	int32_t iSlotNum;
	sItemBase ItemInven;
};

#pragma pack(2)
struct sEPRecord {
	int16_t uiScore;
	int8_t uiRank;
	int8_t uiRing;
	int16_t uiTime;
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

struct sBuddyStyleInfo {
	sPCStyle sBuddyStyle;
	sItemBase aEquip[9];
};

struct sSYSTEMTIME {
	int32_t wYear;
	int32_t wMonth;
	int32_t wDayOfWeek;
	int32_t wDay;
	int32_t wHour;
	int32_t wMinute;
	int32_t wSecond;
	int32_t wMilliseconds;
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

#pragma pack(2)
struct sNano {
	int16_t iID;
	int16_t iSkillID;
	int16_t iStamina;
};

struct sNanoBank {
	int16_t iSkillID;
	int16_t iStamina;
};

#pragma pack(4)
struct sTimeBuff {
	uint64_t iTimeLimit;
	uint64_t iTimeDuration;
	int32_t iTimeRepeat;
	int32_t iValue;
	int32_t iConfirmNum;
};

struct sTimeBuff_Svr {
	uint64_t iTimeLimit;
	uint64_t iTimeDuration;
	int32_t iTimeRepeat;
	int32_t iValue;
	int32_t iConfirmNum;
	int16_t iTimeFlow;
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
	sItemBase aEquip[9];
	sItemBase aInven[50];
	sItemBase aQInven[50];
	sNano aNanoBank[37];
	short aNanoSlots[3];
	int16_t iActiveNanoSlotNum;
	int32_t iConditionBitFlag;
	int32_t eCSTB___Add;
	sTimeBuff TimeBuff;
	long aQuestFlag[32];
	long aRepeatQuestFlag[8];
	sRunningQuest aRunningQuest[9];
	int32_t iCurrentMissionID;
	int32_t iWarpLocationFlag;
	long aWyvernLocationFlag[2];
	int32_t iBuddyWarpTime;
	int32_t iFatigue;
	int32_t iFatigue_Level;
	int32_t iFatigueRate;
	int64_t iFirstUseFlag1;
	int64_t iFirstUseFlag2;
	int aiPCSkill[33];
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
	sItemBase ItemEquip[9];
	sNano Nano;
	int32_t eRT;
};

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

struct sBulletAppearanceData {
	int32_t iBullet_ID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
};

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

struct sTransportationAppearanceData {
	int32_t eTT;
	int32_t iT_ID;
	int32_t iT_Type;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sShinyAppearanceData {
	int32_t iShiny_ID;
	int32_t iShinyType;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sAttackResult {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
	int8_t iHitFlag;
};

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

struct sSkillResult_Damage {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iDamage;
	int32_t iHP;
};

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

struct sSkillResult_Heal_HP {
	int32_t eCT;
	int32_t iID;
	int32_t iHealHP;
	int32_t iHP;
};

struct sSkillResult_Heal_Stamina {
	int32_t eCT;
	int32_t iID;
	int16_t iHealNanoStamina;
	sNano Nano;
};

struct sSkillResult_Stamina_Self {
	int32_t eCT;
	int32_t iID;
	int32_t iReduceHP;
	int32_t iHP;
	int16_t iHealNanoStamina;
	sNano Nano;
};

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

struct sSkillResult_Buff {
	int32_t eCT;
	int32_t iID;
	int32_t bProtected;
	int32_t iConditionBitFlag;
};

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

struct sSkillResult_Move {
	int32_t eCT;
	int32_t iID;
	int32_t iMapNum;
	int32_t iMoveX;
	int32_t iMoveY;
	int32_t iMoveZ;
};

struct sSkillResult_Resurrect {
	int32_t eCT;
	int32_t iID;
	int32_t iRegenHP;
};

struct sPC_HP {
	int32_t iPC_ID;
	int32_t iHP;
};

struct sPC_BATTERYs {
	int32_t iPC_ID;
	int32_t iBatteryW;
	int32_t iBatteryN;
};

struct sPC_NanoSlots {
	int aNanoSlots[3];
	int16_t iActiveNanoSlotNum;
};

struct sPC_Nano {
	int32_t iPC_ID;
	sNano Nano;
	int16_t iActiveNanoSlotNum;
};

struct sPCRegenData {
	int32_t iHP;
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int16_t iActiveNanoSlotNum;
	sNano Nanos[3];
};

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

struct sPCBullet {
	int32_t eAT;
	int32_t iID;
	int32_t bCharged;
};

struct sNPCBullet {
	int32_t eAT;
	int32_t iID;
	int32_t bCharged;
	int32_t eST;
};

struct sNPCLocationData {
	int32_t iNPC_Type;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iRoute;
};

struct sGroupNPCLocationData {
	int32_t iGroupType;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iAngle;
	int32_t iRoute;
	int aGroupNPCIDs[5];
};

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

struct sCNStreetStall_ItemInfo_for_Client {
	int32_t iListNum;
	sItemBase Item;
	int32_t iPrice;
};

#pragma pack(2)
struct sQuickSlot {
	int16_t iType;
	int16_t iID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_ENTER {
	char16_t szID[33];
	int32_t iTempValue;
	int64_t iEnterSerialKey;
};

struct sP_CL2FE_REQ_PC_EXIT {
	int32_t iID;
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

struct sP_CL2FE_REQ_PC_ATTACK_NPCs {
	int32_t iNPCCnt;
};

struct sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_PC_REGEN {
	int32_t iRegenType;
	int32_t eIL;
	int32_t iIndex;
};

struct sP_CL2FE_REQ_ITEM_MOVE {
	int32_t eFrom;
	int32_t iFromSlotNum;
	int32_t eTo;
	int32_t iToSlotNum;
};

struct sP_CL2FE_REQ_PC_TASK_START {
	int32_t iTaskNum;
	int32_t iNPC_ID;
	int32_t iEscortNPC_ID;
};

struct sP_CL2FE_REQ_PC_TASK_END {
	int32_t iTaskNum;
	int32_t iNPC_ID;
	int8_t iBox1Choice;
	int8_t iBox2Choice;
	int32_t iEscortNPC_ID;
};

#pragma pack(2)
struct sP_CL2FE_REQ_NANO_EQUIP {
	int16_t iNanoID;
	int16_t iNanoSlotNum;
};

struct sP_CL2FE_REQ_NANO_UNEQUIP {
	int16_t iNanoSlotNum;
};

struct sP_CL2FE_REQ_NANO_ACTIVE {
	int16_t iNanoSlotNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NANO_TUNE {
	int16_t iNanoID;
	int16_t iTuneID;
	int aiNeedItemSlotNum[10];
};

struct sP_CL2FE_REQ_NANO_SKILL_USE {
	int8_t iBulletID;
	int32_t iArg1;
	int32_t iArg2;
	int32_t iArg3;
	int32_t iTargetCnt;
};

struct sP_CL2FE_REQ_PC_TASK_STOP {
	int32_t iTaskNum;
};

struct sP_CL2FE_REQ_PC_TASK_CONTINUE {
	int32_t iTaskNum;
};

struct sP_CL2FE_REQ_PC_GOTO {
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

struct sP_CL2FE_REQ_CHARGE_NANO_STAMINA {
	int32_t iPC_ID;
};

struct sP_CL2FE_REQ_PC_KILL_QUEST_NPCs {
	int32_t iNPCCnt;
};

struct sP_CL2FE_REQ_PC_VENDOR_ITEM_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
	int32_t iInvenSlotNum;
};

struct sP_CL2FE_REQ_PC_VENDOR_ITEM_SELL {
	int32_t iInvenSlotNum;
	int32_t iItemCnt;
};

struct sP_CL2FE_REQ_PC_ITEM_DELETE {
	int32_t eIL;
	int32_t iSlotNum;
};

struct sP_CL2FE_REQ_PC_GIVE_ITEM {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase Item;
	int32_t iTimeLeft;
};

struct sP_CL2FE_REQ_PC_ROCKET_STYLE_READY {
	int32_t iSkillID;
};

struct sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE {
	int32_t iSkillID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

struct sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT {
	int8_t iBulletID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iTargetCnt;
};

struct sP_CL2FE_REQ_PC_GRENADE_STYLE_READY {
	int32_t iSkillID;
};

struct sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE {
	int32_t iSkillID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
};

struct sP_CL2FE_REQ_PC_GRENADE_STYLE_HIT {
	int8_t iBulletID;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iTargetCnt;
};

struct sP_CL2FE_REQ_PC_NANO_CREATE {
	int16_t iNanoID;
	int32_t iNeedQuestItemSlotNum;
};

struct sP_CL2FE_REQ_PC_TRADE_OFFER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_OFFER_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_OFFER_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int16_t iErrorCode;
};

struct sP_CL2FE_REQ_PC_TRADE_CONFIRM {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_CONFIRM_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item;
};

struct sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item;
};

struct sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iCandy;
};

struct sP_CL2FE_REQ_PC_TRADE_EMOTES_CHAT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int8_t iFreeChatUse;
};

struct sP_CL2FE_REQ_PC_BANK_OPEN {
	int32_t iPC_ID;
};

struct sP_CL2FE_REQ_PC_BANK_CLOSE {
	int32_t iPC_ID;
};

struct sP_CL2FE_REQ_PC_VENDOR_START {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

struct sP_CL2FE_REQ_PC_VENDOR_TABLE_UPDATE {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

struct sP_CL2FE_REQ_PC_VENDOR_ITEM_RESTORE_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
	int32_t iInvenSlotNum;
};

struct sP_CL2FE_REQ_PC_COMBAT_BEGIN {
	int32_t iPC_ID;
};

struct sP_CL2FE_REQ_PC_COMBAT_END {
	int32_t iPC_ID;
};

struct sP_CL2FE_REQ_REQUEST_MAKE_BUDDY {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

struct sP_CL2FE_REQ_ACCEPT_MAKE_BUDDY {
	int8_t iAcceptFlag;
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

struct sP_CL2FE_REQ_SEND_BUDDY_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_CL2FE_REQ_SEND_BUDDY_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_CL2FE_REQ_GET_BUDDY_STYLE {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_CL2FE_REQ_SET_BUDDY_BLOCK {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_CL2FE_REQ_REMOVE_BUDDY {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

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
	int32_t iPlatformID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

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

struct sP_CL2FE_REQ_PC_STATE_CHANGE {
	int32_t iState;
};

struct sP_CL2FE_REQ_PC_MAP_WARP {
	int32_t iMapNum;
};

#pragma pack(2)
struct sP_CL2FE_REQ_PC_GIVE_NANO {
	int16_t iNanoID;
};

#pragma pack(4)
struct sP_CL2FE_REQ_NPC_SUMMON {
	int32_t iNPCType;
	int16_t iNPCCnt;
};

struct sP_CL2FE_REQ_NPC_UNSUMMON {
	int32_t iNPC_ID;
};

struct sP_CL2FE_REQ_ITEM_CHEST_OPEN {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase ChestItem;
};

#pragma pack(2)
struct sP_CL2FE_REQ_PC_GIVE_NANO_SKILL {
	int16_t iNanoID;
	int16_t iNanoSkillID;
};

#pragma pack(4)
struct sP_CL2FE_DOT_DAMAGE_ONOFF {
	int32_t iFlag;
};

struct sP_CL2FE_REQ_PC_VENDOR_BATTERY_BUY {
	int32_t iNPC_ID;
	int32_t iVendorID;
	int8_t iListID;
	sItemBase Item;
};

struct sP_CL2FE_REQ_PC_WARP_USE_NPC {
	int32_t iNPC_ID;
	int32_t iWarpID;
	int32_t eIL1;
	int32_t iItemSlot1;
	int32_t eIL2;
	int32_t iItemSlot2;
};

struct sP_CL2FE_REQ_PC_GROUP_INVITE {
	int32_t iID_To;
};

struct sP_CL2FE_REQ_PC_GROUP_INVITE_REFUSE {
	int32_t iID_From;
};

struct sP_CL2FE_REQ_PC_GROUP_JOIN {
	int32_t iID_From;
};

struct sP_CL2FE_REQ_PC_GROUP_LEAVE {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_PC_BUDDY_WARP {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
};

struct sP_CL2FE_REQ_GET_MEMBER_STYLE {
	int32_t iMemberID;
	int64_t iMemberUID;
};

struct sP_CL2FE_REQ_GET_GROUP_STYLE {
	uint8_t UNUSED;
};

#pragma pack(2)
struct sP_CL2FE_REQ_PC_CHANGE_MENTOR {
	int16_t iMentor;
};

#pragma pack(4)
struct sP_CL2FE_REQ_GET_BUDDY_LOCATION {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
};

struct sP_CL2FE_REQ_NPC_GROUP_SUMMON {
	int32_t iNPCGroupType;
};

struct sP_CL2FE_REQ_PC_WARP_TO_PC {
	int32_t iPC_ID;
	int32_t iPCUID;
};

struct sP_CL2FE_REQ_EP_RANK_GET_LIST {
	int32_t iRankListPageNum;
};

struct sP_CL2FE_REQ_EP_RANK_GET_DETAIL {
	int32_t iEP_ID;
};

struct sP_CL2FE_REQ_EP_RANK_GET_PC_INFO {
	int32_t iEP_ID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

struct sP_CL2FE_REQ_EP_RACE_START {
	int32_t iStartEcomID;
	int32_t iEPRaceMode;
	int32_t iEPTicketItemSlotNum;
};

struct sP_CL2FE_REQ_EP_RACE_END {
	int32_t iEndEcomID;
	int32_t iEPTicketItemSlotNum;
};

struct sP_CL2FE_REQ_EP_RACE_CANCEL {
	int32_t iStartEcomID;
};

struct sP_CL2FE_REQ_EP_GET_RING {
	int32_t iRingLID;
};

struct sP_CL2FE_REQ_IM_CHANGE_SWITCH_STATUS {
	int32_t iSwitchLID;
};

struct sP_CL2FE_REQ_SHINY_PICKUP {
	int32_t iShinyID;
};

struct sP_CL2FE_REQ_SHINY_SUMMON {
	int32_t iShinyType;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

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

struct sP_CL2FE_REQ_SEND_ALL_GROUP_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_SEND_ANY_GROUP_FREECHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iGroupPC_ID;
};

struct sP_CL2FE_REQ_BARKER {
	int32_t iMissionTaskID;
	int32_t iNPC_ID;
};

struct sP_CL2FE_REQ_SEND_ALL_GROUP_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_CL2FE_REQ_SEND_ANY_GROUP_MENUCHAT_MESSAGE {
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iGroupPC_ID;
};

struct sP_CL2FE_REQ_REGIST_TRANSPORTATION_LOCATION {
	int32_t eTT;
	int32_t iNPC_ID;
	int32_t iLocationID;
};

struct sP_CL2FE_REQ_PC_WARP_USE_TRANSPORTATION {
	int32_t iNPC_ID;
	int32_t iTransporationID;
	int32_t eIL;
	int32_t iSlotNum;
};

struct sP_CL2FE_GM_REQ_PC_SPECIAL_STATE_SWITCH {
	int32_t iPC_ID;
	int8_t iSpecialStateFlag;
};

struct sP_CL2FE_GM_REQ_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

struct sP_CL2FE_GM_REQ_KICK_PLAYER {
	int32_t iPC_ID;
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
};

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

struct sP_CL2FE_GM_REQ_PC_LOCATION {
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
};

struct sP_CL2FE_GM_REQ_PC_ANNOUNCE {
	int8_t iAreaType;
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

struct sP_CL2FE_REQ_SET_PC_BLOCK {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
};

struct sP_CL2FE_REQ_REGIST_RXCOM {
	int32_t iNPCID;
};

#pragma pack(2)
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

struct sP_CL2FE_REQ_WARP_USE_RECALL {
	int32_t iGroupMemberID;
};

struct sP_CL2FE_REP_LIVE_CHECK {
	int32_t iTempValue;
};

struct sP_CL2FE_REQ_PC_MISSION_COMPLETE {
	int32_t iMissionNum;
};

struct sP_CL2FE_REQ_PC_TASK_COMPLETE {
	int32_t iTaskNum;
};

struct sP_CL2FE_REQ_NPC_INTERACTION {
	int32_t iNPC_ID;
	int32_t bFlag;
};

struct sP_CL2FE_DOT_HEAL_ONOFF {
	int32_t iFlag;
};

struct sP_CL2FE_REQ_PC_SPECIAL_STATE_SWITCH {
	int32_t iPC_ID;
	int8_t iSpecialStateFlag;
};

struct sP_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_READ_EMAIL {
	int64_t iEmailIndex;
};

#pragma pack(1)
struct sP_CL2FE_REQ_PC_RECV_EMAIL_PAGE_LIST {
	int8_t iPageNum;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_DELETE_EMAIL {
	long iEmailIndexArray[5];
};

struct sP_CL2FE_REQ_PC_SEND_EMAIL {
	int64_t iTo_PCUID;
	char16_t szSubject[32];
	char16_t szContent[512];
	sEmailItemInfoFromCL aItem[4];
	int32_t iCash;
};

struct sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
};

struct sP_CL2FE_REQ_PC_RECV_EMAIL_CANDY {
	int64_t iEmailIndex;
};

struct sP_CL2FE_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF {
	int32_t eTargetSearchBy;
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int64_t iTargetPC_UID;
	int32_t iONOFF;
	int8_t iSpecialStateFlag;
};

struct sP_CL2FE_REQ_PC_SET_CURRENT_MISSION_ID {
	int32_t iCurrentMissionID;
};

struct sP_CL2FE_REQ_NPC_GROUP_INVITE {
	int32_t iNPC_ID;
};

struct sP_CL2FE_REQ_NPC_GROUP_KICK {
	int32_t iNPC_ID;
};

struct sP_CL2FE_REQ_PC_FIRST_USE_FLAG_SET {
	int32_t iFlagCode;
};

struct sP_CL2FE_REQ_PC_TRANSPORT_WARP {
	int32_t iTransport_ID;
	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;
};

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

struct sP_CL2FE_REQ_PC_RECV_EMAIL_ITEM_ALL {
	int64_t iEmailIndex;
};

struct sP_CL2FE_REQ_CHANNEL_INFO {
	uint8_t UNUSED;
};

struct sP_CL2FE_REQ_PC_CHANNEL_NUM {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_WARP_CHANNEL {
	int32_t iChannelNum;
	int8_t iWarpType;
};

struct sP_CL2FE_REQ_PC_LOADING_COMPLETE {
	int32_t iPC_ID;
};

#pragma pack(2)
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

struct sP_CL2FE_REQ_PC_ATTACK_CHARs {
	int32_t iTargetCnt;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_READY {
	int32_t iStreetStallItemInvenSlotNum;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_CANCEL {
	int32_t iPC_ID;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_REGIST_ITEM {
	int32_t iItemListNum;
	int32_t iItemInvenSlotNum;
	sItemBase Item;
	int32_t iPrice;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_UNREGIST_ITEM {
	int32_t iItemListNum;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_SALE_START {
	int32_t iStreetStallItemInvenSlotNum;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_ITEM_LIST {
	int32_t iStreetStallPC_ID;
};

struct sP_CL2FE_PC_STREETSTALL_REQ_ITEM_BUY {
	int32_t iStreetStallPC_ID;
	int32_t iItemListNum;
	int32_t iEmptyInvenSlotNum;
};

struct sP_CL2FE_REQ_PC_ITEM_COMBINATION {
	int32_t iCostumeItemSlot;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

struct sP_CL2FE_GM_REQ_SET_PC_SKILL {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

struct sP_CL2FE_REQ_PC_SKILL_ADD {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iSkillItemInvenSlotNum;
	int32_t iPreSkillSlotNum;
	int32_t iPreSkillID;
};

struct sP_CL2FE_REQ_PC_SKILL_DEL {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

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

struct sP_CL2FE_REQ_PC_ROPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iRopeID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

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
	int32_t iBeltID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
};

struct sP_CL2FE_REQ_PC_VEHICLE_ON {
	uint8_t UNUSED;
};

struct sP_CL2FE_REQ_PC_VEHICLE_OFF {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2FE_REQ_PC_REGIST_QUICK_SLOT {
	int32_t iSlotNum;
	int16_t iItemType;
	int16_t iItemID;
};

struct sP_CL2FE_REQ_PC_DISASSEMBLE_ITEM {
	int32_t iItemSlot;
};

struct sP_CL2FE_GM_REQ_REWARD_RATE {
	int32_t iGetSet;
	int32_t iRewardType;
	int32_t iRewardRateIndex;
	int32_t iSetRateValue;
};

struct sP_CL2FE_REQ_PC_ITEM_ENCHANT {
	int32_t iEnchantItemSlot;
	int32_t iWeaponMaterialItemSlot;
	int32_t iDefenceMaterialItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

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

struct sP_CL2LS_REQ_CHAR_DELETE {
	int64_t iPC_UID;
};

#pragma pack(1)
struct sP_CL2LS_REQ_SHARD_SELECT {
	int8_t ShardNum;
};

struct sP_CL2LS_REQ_SHARD_LIST_INFO {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_CL2LS_CHECK_NAME_LIST {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
};

struct sP_CL2LS_REQ_SAVE_CHAR_TUTOR {
	int64_t iPC_UID;
	int8_t iTutorialFlag;
};

#pragma pack(2)
struct sP_CL2LS_REQ_PC_EXIT_DUPLICATE {
	char16_t szID[33];
	char16_t szPassword[33];
};

#pragma pack(4)
struct sP_CL2LS_REP_LIVE_CHECK {
	int32_t iTempValue;
};

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

#pragma pack(1)
struct sP_CL2LS_REQ_SERVER_SELECT {
	int8_t ServerNum;
};

#pragma pack(4)
struct sPacket {
	uint32_t dwType;
	uint8_t szData[4096];
};

struct sPacket_Full {
	uint32_t dwSize;
	uint32_t dwType;
	uint8_t szData[4096];
};

struct sPacket2x {
	uint32_t dwType;
	uint8_t szData[8192];
};

struct sPacket2x_Full {
	uint32_t dwSize;
	uint32_t dwType;
	uint8_t szData[8192];
};

struct sP_FE2CL_ERROR {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_ENTER_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_ENTER_SUCC {
	int32_t iID;
	sPCLoadData2CL PCLoadData2CL;
	uint64_t uiSvrTime;
};

struct sP_FE2CL_PC_NEW {
	sPCAppearanceData PCAppearanceData;
};

struct sP_FE2CL_REP_PC_EXIT_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_EXIT_SUCC {
	int32_t iID;
	int32_t iExitCode;
};

struct sP_FE2CL_PC_EXIT {
	int32_t iID;
	int32_t iExitType;
};

struct sP_FE2CL_PC_AROUND {
	int32_t iPCCnt;
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

struct sP_FE2CL_NPC_ENTER {
	sNPCAppearanceData NPCAppearanceData;
};

struct sP_FE2CL_NPC_EXIT {
	int32_t iNPC_ID;
};

struct sP_FE2CL_NPC_MOVE {
	int32_t iNPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
	int16_t iMoveStyle;
};

struct sP_FE2CL_NPC_NEW {
	sNPCAppearanceData NPCAppearanceData;
};

struct sP_FE2CL_NPC_AROUND {
	int32_t iNPCCnt;
};

struct sP_FE2CL_AROUND_DEL_PC {
	int32_t iPCCnt;
};

struct sP_FE2CL_AROUND_DEL_NPC {
	int32_t iNPCCnt;
};

struct sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC {
	int32_t iPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_PC_ATTACK_NPCs_SUCC {
	int32_t iBatteryW;
	int32_t iNPCCnt;
};

struct sP_FE2CL_PC_ATTACK_NPCs {
	int32_t iPC_ID;
	int32_t iNPCCnt;
};

struct sP_FE2CL_NPC_ATTACK_PCs {
	int32_t iNPC_ID;
	int32_t iPCCnt;
};

struct sP_FE2CL_REP_PC_REGEN_SUCC {
	sPCRegenData PCRegenData;
	int32_t bMoveLocation;
	int32_t iFusionMatter;
};

struct sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC {
	int32_t iPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_PC_ITEM_MOVE_SUCC {
	int32_t eFrom;
	int32_t iFromSlotNum;
	sItemBase FromSlotItem;
	int32_t eTo;
	int32_t iToSlotNum;
	sItemBase ToSlotItem;
};

struct sP_FE2CL_PC_EQUIP_CHANGE {
	int32_t iPC_ID;
	int32_t iEquipSlotNum;
	sItemBase EquipSlotItem;
};

struct sP_FE2CL_REP_PC_TASK_START_SUCC {
	int32_t iTaskNum;
	int32_t iRemainTime;
};

struct sP_FE2CL_REP_PC_TASK_START_FAIL {
	int32_t iTaskNum;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TASK_END_SUCC {
	int32_t iTaskNum;
};

struct sP_FE2CL_REP_PC_TASK_END_FAIL {
	int32_t iTaskNum;
	int32_t iErrorCode;
};

struct sP_FE2CL_NPC_SKILL_READY {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
};

struct sP_FE2CL_NPC_SKILL_FIRE {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iVX;
	int32_t iVY;
	int32_t iVZ;
};

struct sP_FE2CL_NPC_SKILL_HIT {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
	int32_t eST;
	int32_t iTargetCnt;
};

struct sP_FE2CL_NPC_SKILL_CORRUPTION_READY {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int16_t iStyle;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
};

struct sP_FE2CL_NPC_SKILL_CORRUPTION_HIT {
	int32_t iNPC_ID;
	int16_t iSkillID;
	int16_t iStyle;
	int32_t iValue1;
	int32_t iValue2;
	int32_t iValue3;
	int32_t iTargetCnt;
};

struct sP_FE2CL_NPC_SKILL_CANCEL {
	int32_t iNPC_ID;
};

struct sP_FE2CL_REP_NANO_EQUIP_SUCC {
	int16_t iNanoID;
	int16_t iNanoSlotNum;
	int32_t bNanoDeactive;
};

struct sP_FE2CL_REP_NANO_UNEQUIP_SUCC {
	int16_t iNanoSlotNum;
	int32_t bNanoDeactive;
};

struct sP_FE2CL_REP_NANO_ACTIVE_SUCC {
	int16_t iActiveNanoSlotNum;
	int32_t eCSTB___Add;
};

struct sP_FE2CL_REP_NANO_TUNE_SUCC {
	int16_t iNanoID;
	int16_t iSkillID;
	int32_t iPC_FusionMatter;
	int aiItemSlotNum[10];
	sItemBase aItem[10];
};

struct sP_FE2CL_NANO_ACTIVE {
	int32_t iPC_ID;
	sNano Nano;
	int32_t iConditionBitFlag;
	int32_t eCSTB___Add;
};

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

struct sP_FE2CL_REP_PC_TASK_STOP_SUCC {
	int32_t iTaskNum;
};

struct sP_FE2CL_REP_PC_TASK_STOP_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TASK_CONTINUE_SUCC {
	int32_t iTaskNum;
};

struct sP_FE2CL_REP_PC_TASK_CONTINUE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_GOTO_SUCC {
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sP_FE2CL_REP_CHARGE_NANO_STAMINA {
	int32_t iBatteryN;
	int16_t iNanoID;
	int16_t iNanoStamina;
};

struct sP_FE2CL_REP_PC_TICK {
	int32_t iHP;
	sNano aNano[3];
	int32_t iBatteryN;
	int32_t bResetMissionFlag;
};

struct sP_FE2CL_REP_PC_KILL_QUEST_NPCs_SUCC {
	int32_t iNPCID;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_BUY_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
	sItemBase ItemStay;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_SELL_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_ITEM_DELETE_SUCC {
	int32_t eIL;
	int32_t iSlotNum;
};

struct sP_FE2CL_PC_ROCKET_STYLE_READY {
	int32_t iPC_ID;
	int32_t iSkillID;
};

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

struct sP_FE2CL_PC_ROCKET_STYLE_HIT {
	int32_t iPC_ID;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iTargetCnt;
};

struct sP_FE2CL_PC_GRENADE_STYLE_READY {
	int32_t iPC_ID;
	int32_t iSkillID;
};

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

struct sP_FE2CL_PC_GRENADE_STYLE_FIRE {
	int32_t iPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t bNanoDeactive;
};

struct sP_FE2CL_PC_GRENADE_STYLE_HIT {
	int32_t iPC_ID;
	int8_t iBulletID;
	sPCBullet Bullet;
	int32_t iTargetCnt;
};

struct sP_FE2CL_REP_PC_TRADE_OFFER {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_OFFER_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_OFFER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_OFFER_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int16_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TRADE_CONFIRM {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
};

struct sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade Item[12];
	int32_t iCandy;
	sItemTrade ItemStay[12];
};

struct sP_FE2CL_REP_PC_TRADE_CONFIRM_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade TradeItem;
	sItemTrade InvenItem;
};

struct sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	sItemTrade TradeItem;
	sItemTrade InvenItem;
};

struct sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iCandy;
};

struct sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_PC_NANO_CREATE_SUCC {
	int32_t iPC_FusionMatter;
	int32_t iQuestItemSlotNum;
	sItemBase QuestItem;
	sNano Nano;
	int16_t iPC_Level;
};

struct sP_FE2CL_REP_PC_NANO_CREATE_FAIL {
	int32_t iPC_ID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_NANO_TUNE_FAIL {
	int32_t iPC_ID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_BANK_OPEN_SUCC {
	sItemBase aBank[119];
	int32_t iExtraBank;
};

struct sP_FE2CL_REP_PC_BANK_OPEN_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_BANK_CLOSE_SUCC {
	int32_t iPC_ID;
};

struct sP_FE2CL_REP_PC_BANK_CLOSE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_VENDOR_START_SUCC {
	int32_t iNPC_ID;
	int32_t iVendorID;
};

struct sP_FE2CL_REP_PC_VENDOR_START_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_SUCC {
	sItemVendor item[20];
};

struct sP_FE2CL_REP_PC_VENDOR_TABLE_UPDATE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_SUCC {
	int32_t iCandy;
	int32_t iInvenSlotNum;
	sItemBase Item;
};

struct sP_FE2CL_REP_PC_VENDOR_ITEM_RESTORE_BUY_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_CHAR_TIME_BUFF_TIME_OUT {
	int32_t eCT;
	int32_t iID;
	int32_t iConditionBitFlag;
};

struct sP_FE2CL_REP_PC_GIVE_ITEM_SUCC {
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase Item;
};

struct sP_FE2CL_REP_PC_GIVE_ITEM_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_BUDDYLIST_INFO_SUCC {
	int32_t iID;
	int64_t iPCUID;
	int8_t iListNum;
	int8_t iBuddyCnt;
};

struct sP_FE2CL_REP_PC_BUDDYLIST_INFO_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC {
	int32_t iRequestID;
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
};

struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_FAIL {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_SUCC {
	int8_t iBuddySlot;
	sBuddyBaseInfo BuddyInfo;
};

struct sP_FE2CL_REP_ACCEPT_MAKE_BUDDY_FAIL {
	int32_t iBuddyID;
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_SUCC {
	int64_t iFromPCUID;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_BUDDY_FREECHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_SUCC {
	int64_t iFromPCUID;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_BUDDY_MENUCHAT_MESSAGE_FAIL {
	int32_t iErrorCode;
	int64_t iToPCUID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_GET_BUDDY_STYLE_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
	sBuddyStyleInfo sBuddyStyle;
};

struct sP_FE2CL_REP_GET_BUDDY_STYLE_FAIL {
	int32_t iErrorCode;
	int64_t iBuddyPCUID;
};

struct sP_FE2CL_REP_GET_BUDDY_STATE_SUCC {
	int aBuddyID[50];
	uint8_t aBuddyState[50];
};

struct sP_FE2CL_REP_GET_BUDDY_STATE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_SET_BUDDY_BLOCK_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_FE2CL_REP_SET_BUDDY_BLOCK_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_REMOVE_BUDDY_SUCC {
	int64_t iBuddyPCUID;
	int8_t iBuddySlot;
};

struct sP_FE2CL_REP_REMOVE_BUDDY_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

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
	int32_t iPlatformID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

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

struct sP_FE2CL_PC_STATE_CHANGE {
	int32_t iPC_ID;
	int8_t iState;
};

struct sP_FE2CL_REP_REQUEST_MAKE_BUDDY_SUCC_TO_ACCEPTER {
	int32_t iRequestID;
	int32_t iBuddyID;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

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

struct sP_FE2CL_REP_ITEM_CHEST_OPEN_SUCC {
	int32_t iSlotNum;
};

struct sP_FE2CL_REP_ITEM_CHEST_OPEN_FAIL {
	int32_t iSlotNum;
	int32_t iErrorCode;
};

struct sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK {
	int32_t eCT;
	int32_t iID;
	int16_t iTB_ID;
};

struct sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_SUCC {
	int32_t iCandy;
	int32_t iBatteryW;
	int32_t iBatteryN;
};

struct sP_FE2CL_REP_PC_VENDOR_BATTERY_BUY_FAIL {
	int32_t iErrorCode;
};

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

struct sP_FE2CL_NPC_GRENADE_STYLE_FIRE {
	int32_t iNPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int8_t iBulletID;
	sNPCBullet Bullet;
};

struct sP_FE2CL_NPC_BULLET_STYLE_HIT {
	int32_t iNPC_ID;
	int8_t iBulletID;
	sNPCBullet Bullet;
	int32_t iTargetCnt;
};

struct sP_FE2CL_CHARACTER_ATTACK_CHARACTERs {
	int32_t eCT;
	int32_t iCharacterID;
	int32_t iTargetCnt;
};

struct sP_FE2CL_PC_GROUP_INVITE {
	int32_t iHostID;
};

struct sP_FE2CL_PC_GROUP_INVITE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_GROUP_INVITE_REFUSE {
	int32_t iID_To;
};

struct sP_FE2CL_PC_GROUP_JOIN {
	int32_t iID_NewMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_PC_GROUP_JOIN_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_GROUP_JOIN_SUCC {
	int32_t iID_NewMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_PC_GROUP_LEAVE {
	int32_t iID_LeaveMember;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_PC_GROUP_LEAVE_FAIL {
	int32_t iID;
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_GROUP_LEAVE_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_GROUP_MEMBER_INFO {
	int32_t iID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_REP_PC_WARP_USE_NPC_SUCC {
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t eIL;
	int32_t iItemSlotNum;
	sItemBase Item;
	int32_t iCandy;
};

struct sP_FE2CL_REP_PC_WARP_USE_NPC_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT {
	int32_t iID_From;
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_PC_CHANGE_MENTOR_SUCC {
	int16_t iMentor;
	int16_t iMentorCnt;
	int32_t iFusionMatter;
};

struct sP_FE2CL_REP_PC_CHANGE_MENTOR_FAIL {
	int16_t iMentor;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_GET_MEMBER_STYLE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_GET_MEMBER_STYLE_SUCC {
	int32_t iMemberID;
	int64_t iMemberUID;
	sBuddyStyleInfo BuddyStyleInfo;
};

struct sP_FE2CL_REP_GET_GROUP_STYLE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_GET_GROUP_STYLE_SUCC {
	int32_t iMemberCnt;
};

struct sP_FE2CL_PC_REGEN {
	sPCRegenDataForOtherPC PCRegenDataForOtherPC;
};

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

struct sP_FE2CL_TRANSPORTATION_ENTER {
	sTransportationAppearanceData AppearanceData;
};

struct sP_FE2CL_TRANSPORTATION_EXIT {
	int32_t eTT;
	int32_t iT_ID;
};

struct sP_FE2CL_TRANSPORTATION_MOVE {
	int32_t eTT;
	int32_t iT_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
	int16_t iMoveStyle;
};

struct sP_FE2CL_TRANSPORTATION_NEW {
	sTransportationAppearanceData AppearanceData;
};

struct sP_FE2CL_TRANSPORTATION_AROUND {
	int32_t iCnt;
};

struct sP_FE2CL_AROUND_DEL_TRANSPORTATION {
	int32_t eTT;
	int32_t iCnt;
};

struct sP_FE2CL_REP_EP_RANK_LIST {
	uint8_t UNUSED;
};

struct sP_FE2CL_REP_EP_RANK_DETAIL {
	uint8_t UNUSED;
};

struct sP_FE2CL_REP_EP_RANK_PC_INFO {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_EP_RACE_START_SUCC {
	uint64_t iStartTick;
	int32_t iLimitTime;
};

struct sP_FE2CL_REP_EP_RACE_START_FAIL {
	int32_t iErrorCode;
};

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

struct sP_FE2CL_REP_EP_RACE_END_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_EP_RACE_CANCEL_SUCC {
	int32_t iTemp;
};

struct sP_FE2CL_REP_EP_RACE_CANCEL_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_EP_GET_RING_SUCC {
	int32_t iRingLID;
	int32_t iRingCount_Get;
};

struct sP_FE2CL_REP_EP_GET_RING_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_IM_CHANGE_SWITCH_STATUS {
	int32_t iMapNum;
	int32_t iSwitchLID;
	int32_t iSwitchGID;
	int32_t iSwitchStatus;
};

struct sP_FE2CL_SHINY_ENTER {
	sShinyAppearanceData ShinyAppearanceData;
};

struct sP_FE2CL_SHINY_EXIT {
	int32_t iShinyID;
};

struct sP_FE2CL_SHINY_NEW {
	sShinyAppearanceData ShinyAppearanceData;
};

struct sP_FE2CL_SHINY_AROUND {
	int32_t iShinyCnt;
};

struct sP_FE2CL_AROUND_DEL_SHINY {
	int32_t iShinyCnt;
};

struct sP_FE2CL_REP_SHINY_PICKUP_FAIL {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_REP_SHINY_PICKUP_SUCC {
	int16_t iSkillID;
	int32_t eCSTB;
};

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

struct sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_ALL_GROUP_FREECHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_ANY_GROUP_FREECHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_BARKER {
	int32_t iNPC_ID;
	int32_t iMissionStringID;
};

struct sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_ALL_GROUP_MENUCHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_SUCC {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
};

struct sP_FE2CL_REP_SEND_ANY_GROUP_MENUCHAT_MESSAGE_FAIL {
	int32_t iSendPCID;
	int32_t iGroupPC_ID;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_FAIL {
	int32_t eTT;
	int32_t iLocationID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC {
	int32_t eTT;
	int32_t iLocationID;
	int32_t iWarpLocationFlag;
	long aWyvernLocationFlag[2];
};

struct sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_FAIL {
	int32_t iTransportationID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_WARP_USE_TRANSPORTATION_SUCC {
	int32_t eTT;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int32_t iCandy;
};

struct sP_FE2CL_ANNOUNCE_MSG {
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

struct sP_FE2CL_REP_PC_SPECIAL_STATE_SWITCH_SUCC {
	int32_t iPC_ID;
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

struct sP_FE2CL_PC_SPECIAL_STATE_CHANGE {
	int32_t iPC_ID;
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

struct sP_FE2CL_GM_REP_PC_SET_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

struct sP_FE2CL_GM_PC_CHANGE_VALUE {
	int32_t iPC_ID;
	int32_t iSetValueType;
	int32_t iSetValue;
};

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

struct sP_FE2CL_GM_REP_PC_ANNOUNCE {
	int8_t iAnnounceType;
	int32_t iDuringTime;
	char16_t szAnnounceMsg[512];
};

struct sP_FE2CL_REP_PC_BUDDY_WARP_FAIL {
	int64_t iBuddyPCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_CHANGE_LEVEL {
	int32_t iPC_ID;
	int16_t iPC_Level;
};

struct sP_FE2CL_REP_SET_PC_BLOCK_SUCC {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
	int8_t iBuddySlot;
};

struct sP_FE2CL_REP_SET_PC_BLOCK_FAIL {
	int32_t iBlock_ID;
	int64_t iBlock_PCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_REGIST_RXCOM {
	int32_t iMapNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
};

struct sP_FE2CL_REP_REGIST_RXCOM_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_INVEN_FULL_MSG {
	int8_t iType;
	int32_t iErrorCode;
};

struct sP_FE2CL_REQ_LIVE_CHECK {
	int32_t iTempValue;
};

#pragma pack(2)
struct sP_FE2CL_PC_MOTD_LOGIN {
	int8_t iType;
	char16_t szSystemMsg[512];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_ITEM_USE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_ITEM_USE_SUCC {
	int32_t iPC_ID;
	int32_t eIL;
	int32_t iSlotNum;
	sItemBase RemainItem;
	int16_t iSkillID;
	int32_t eST;
	int32_t iTargetCnt;
};

struct sP_FE2CL_PC_ITEM_USE {
	int32_t iPC_ID;
	int16_t iSkillID;
	int32_t eST;
	int32_t iTargetCnt;
};

struct sP_FE2CL_REP_GET_BUDDY_LOCATION_SUCC {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	int8_t iShardNum;
};

struct sP_FE2CL_REP_GET_BUDDY_LOCATION_FAIL {
	int64_t iBuddyPCUID;
	int8_t iSlotNum;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RIDING_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RIDING_SUCC {
	int32_t iPC_ID;
	int32_t eRT;
};

struct sP_FE2CL_PC_RIDING {
	int32_t iPC_ID;
	int32_t eRT;
};

struct sP_FE2CL_PC_BROOMSTICK_MOVE {
	int32_t iPC_ID;
	int32_t iToX;
	int32_t iToY;
	int32_t iToZ;
	int32_t iSpeed;
};

struct sP_FE2CL_REP_PC_BUDDY_WARP_OTHER_SHARD_SUCC {
	int64_t iBuddyPCUID;
	int8_t iShardNum;
	int32_t iChannelNum;
};

struct sP_FE2CL_REP_WARP_USE_RECALL_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_EXIT_DUPLICATE {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_MISSION_COMPLETE_SUCC {
	int32_t iMissionNum;
};

struct sP_FE2CL_PC_BUFF_UPDATE {
	int32_t eCSTB;
	int32_t eTBU;
	int32_t eTBT;
	sTimeBuff TimeBuff;
	int32_t iConditionBitFlag;
};

struct sP_FE2CL_REP_PC_NEW_EMAIL {
	int32_t iNewEmailCnt;
};

struct sP_FE2CL_REP_PC_READ_EMAIL_SUCC {
	int64_t iEmailIndex;
	char16_t szContent[512];
	sItemBase aItem[4];
	int32_t iCash;
};

struct sP_FE2CL_REP_PC_READ_EMAIL_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_SUCC {
	int8_t iPageNum;
	sEmailInfo aEmailInfo[5];
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_PAGE_LIST_FAIL {
	int8_t iPageNum;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_DELETE_EMAIL_SUCC {
	long iEmailIndexArray[5];
};

struct sP_FE2CL_REP_PC_DELETE_EMAIL_FAIL {
	long iEmailIndexArray[5];
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_SEND_EMAIL_SUCC {
	int64_t iTo_PCUID;
	int32_t iCandy;
	sEmailItemInfoFromCL aItem[4];
};

struct sP_FE2CL_REP_PC_SEND_EMAIL_FAIL {
	int64_t iTo_PCUID;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_SUCC {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_FAIL {
	int64_t iEmailIndex;
	int32_t iSlotNum;
	int32_t iEmailItemSlot;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_SUCC {
	int64_t iEmailIndex;
	int32_t iCandy;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_CANDY_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_SUDDEN_DEAD {
	int32_t iPC_ID;
	int32_t iSuddenDeadReason;
	int32_t iDamage;
	int32_t iHP;
};

struct sP_FE2CL_REP_GM_REQ_TARGET_PC_SPECIAL_STATE_ONOFF_SUCC {
	int32_t iTargetPC_ID;
	char16_t szTargetPC_FirstName[10];
	char16_t szTargetPC_LastName[18];
	int8_t iReqSpecialStateFlag;
	int8_t iSpecialState;
};

struct sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID {
	int32_t iCurrentMissionID;
};

struct sP_FE2CL_REP_NPC_GROUP_INVITE_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_NPC_GROUP_INVITE_SUCC {
	int32_t iPC_ID;
	int32_t iNPC_ID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_REP_NPC_GROUP_KICK_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_NPC_GROUP_KICK_SUCC {
	int32_t iPC_ID;
	int32_t iNPC_ID;
	int32_t iMemberPCCnt;
	int32_t iMemberNPCCnt;
};

struct sP_FE2CL_PC_EVENT {
	int32_t iPC_ID;
	int32_t iEventID;
	int32_t iEventValue1;
	int32_t iEventValue2;
	int32_t iEventValue3;
};

struct sP_FE2CL_REP_PC_TRANSPORT_WARP_SUCC {
	sTransportationAppearanceData TransportationAppearanceData;
	int32_t iLcX;
	int32_t iLcY;
	int32_t iLcZ;
};

struct sP_FE2CL_REP_PC_TRADE_EMOTES_CHAT_FAIL {
	int32_t iID_Request;
	int32_t iID_From;
	int32_t iID_To;
	char16_t szFreeChat[128];
	int32_t iEmoteCode;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_SUCC {
	int64_t iEmailIndex;
};

struct sP_FE2CL_REP_PC_RECV_EMAIL_ITEM_ALL_FAIL {
	int64_t iEmailIndex;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_LOADING_COMPLETE_SUCC {
	int32_t iPC_ID;
};

struct sChannelInfo {
	int32_t iChannelNum;
	int32_t iCurrentUserCnt;
};

struct sP_FE2CL_REP_CHANNEL_INFO {
	int32_t iCurrChannelNum;
	int32_t iChannelCnt;
};

struct sP_FE2CL_REP_PC_CHANNEL_NUM {
	int32_t iChannelNum;
};

struct sP_FE2CL_REP_PC_WARP_CHANNEL_FAIL {
	int32_t iErrorCode;
};

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

struct sP_FE2CL_REP_PC_FIND_NAME_MAKE_BUDDY_FAIL {
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_FIND_NAME_ACCEPT_BUDDY_FAIL {
	char16_t szFirstName[9];
	char16_t szLastName[17];
	int64_t iPCUID;
	int8_t iNameCheckFlag;
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_BUDDY_WARP_SAME_SHARD_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_ATTACK_CHARs_SUCC {
	int32_t iBatteryW;
	int32_t iTargetCnt;
};

struct sP_FE2CL_PC_ATTACK_CHARs {
	int32_t iPC_ID;
	int32_t iTargetCnt;
};

struct sP_FE2CL_NPC_ATTACK_CHARs {
	int32_t iNPC_ID;
	int32_t iTargetCnt;
};

struct sP_FE2CL_REP_PC_CHANGE_LEVEL_SUCC {
	int32_t iLevel;
	int32_t iFusionMatter;
};

struct sP_FE2CL_REP_PC_NANO_CREATE {
	int32_t iPC_ID;
	int16_t iNanoID;
};

struct sP_FE2CL_PC_STREETSTALL_REP_READY_SUCC {
	int32_t iStreetStallItemInvenSlotNum;
	int32_t iItemListCountMax;
	float fTaxPercentage;
	int8_t iPCCharState;
};

struct sP_FE2CL_PC_STREETSTALL_REP_READY_FAIL {
	int32_t iErrorCode;
};

#pragma pack(1)
struct sP_FE2CL_PC_STREETSTALL_REP_CANCEL_SUCC {
	int8_t iPCCharState;
};

#pragma pack(4)
struct sP_FE2CL_PC_STREETSTALL_REP_CANCEL_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_SUCC {
	int32_t iItemListNum;
	int32_t iItemInvenSlotNum;
	sItemBase Item;
	int32_t iPrice;
};

struct sP_FE2CL_PC_STREETSTALL_REP_REGIST_ITEM_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_SUCC {
	int32_t iItemListNum;
};

struct sP_FE2CL_PC_STREETSTALL_REP_UNREGIST_ITEM_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_STREETSTALL_REP_SALE_START_SUCC {
	int32_t iStreetStallItemInvenSlotNum;
	sItemBase OpenItem;
	int32_t ePCCharState;
};

struct sP_FE2CL_PC_STREETSTALL_REP_SALE_START_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST {
	int32_t iStreetStallPC_ID;
	int32_t iItemListCount;
};

struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_LIST_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_BUYER {
	int32_t iStreetStallPC_ID;
	int32_t iPC_Candy;
	int32_t iPC_ItemInvenSlotNum;
	sItemBase PC_Item;
	int32_t iItemListNum;
};

struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_SUCC_SELLER {
	int32_t iBuyerPC_ID;
	int32_t iStreetStallPC_Candy;
	int32_t iStreetStallPC_ItemInvenSlotNum;
	sItemBase StreetStallPC_Item;
	int32_t iItemListNum;
};

struct sP_FE2CL_PC_STREETSTALL_REP_ITEM_BUY_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_ITEM_COMBINATION_SUCC {
	int32_t iNewItemSlot;
	sItemBase sNewItem;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
	int32_t iCandy;
	int32_t iSuccessFlag;
};

struct sP_FE2CL_REP_PC_ITEM_COMBINATION_FAIL {
	int32_t iErrorCode;
	int32_t iCostumeItemSlot;
	int32_t iStatItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

struct sP_FE2CL_PC_CASH_BUFF_UPDATE {
	int32_t eCSTB;
	int32_t eTBU;
	sTimeBuff TimeBuff;
	int32_t iConditionBitFlag;
};

struct sP_FE2CL_REP_PC_SKILL_ADD_SUCC {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
	int32_t iSkillItemInvenSlotNum;
	sItemBase SkillItem;
};

struct sP_FE2CL_REP_PC_SKILL_ADD_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_SKILL_DEL_SUCC {
	int32_t iSkillSlotNum;
	int32_t iSkillID;
};

struct sP_FE2CL_REP_PC_SKILL_DEL_FAIL {
	int32_t iErrorCode;
};

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

struct sP_FE2CL_REP_PC_SKILL_USE_FAIL {
	int32_t iErrorCode;
};

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

struct sP_FE2CL_PC_ROPE {
	uint64_t iCliTime;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	float fVX;
	float fVY;
	float fVZ;
	int32_t iRopeID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

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
	int32_t iBeltID;
	int32_t iAngle;
	uint8_t cKeyValue;
	int32_t iSpeed;
	int32_t iPC_ID;
	uint64_t iSvrTime;
};

struct sP_FE2CL_PC_VEHICLE_ON_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_ON_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_PC_VEHICLE_OFF_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_FE2CL_PC_VEHICLE_OFF_FAIL {
	int32_t iErrorCode;
};

#pragma pack(2)
struct sP_FE2CL_PC_QUICK_SLOT_INFO {
	sQuickSlot aQuickSlot[8];
};

#pragma pack(4)
struct sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_FAIL {
	int32_t iErrorCode;
};

struct sP_FE2CL_REP_PC_REGIST_QUICK_SLOT_SUCC {
	int32_t iSlotNum;
	int16_t iItemType;
	int16_t iItemID;
};

struct sP_FE2CL_PC_DELETE_TIME_LIMIT_ITEM {
	int32_t iItemListCount;
};

struct sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_SUCC {
	int32_t iNewItemSlot;
	sItemBase sNewItem;
};

struct sP_FE2CL_REP_PC_DISASSEMBLE_ITEM_FAIL {
	int32_t iErrorCode;
	int32_t iItemSlot;
};

struct sP_FE2CL_GM_REP_REWARD_RATE_SUCC {
	float afRewardRate_Taros[5];
	float afRewardRate_FusionMatter[5];
};

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

struct sP_FE2CL_REP_PC_ITEM_ENCHANT_FAIL {
	int32_t iErrorCode;
	int32_t iEnchantItemSlot;
	int32_t iWeaponMaterialItemSlot;
	int32_t iDefenceMaterialItemSlot;
	int32_t iCashItemSlot1;
	int32_t iCashItemSlot2;
};

struct sP_LS2CL_REP_LOGIN_SUCC {
	int8_t iCharCount;
	int8_t iSlotNum;
	int8_t iPaymentFlag;
	int8_t iTempForPacking4;
	uint64_t uiSvrTime;
	char16_t szID[33];
	int32_t iOpenBetaFlag;
};

struct sP_LS2CL_REP_LOGIN_FAIL {
	int32_t iErrorCode;
	char16_t szID[33];
};

struct sP_LS2CL_REP_CHAR_INFO {
	int8_t iSlot;
	int16_t iLevel;
	sPCStyle sPC_Style;
	sPCStyle2 sPC_Style2;
	int32_t iX;
	int32_t iY;
	int32_t iZ;
	sItemBase aEquip[9];
};

#pragma pack(2)
struct sP_LS2CL_REP_CHECK_CHAR_NAME_SUCC {
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

#pragma pack(4)
struct sP_LS2CL_REP_CHECK_CHAR_NAME_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_SAVE_CHAR_NAME_SUCC {
	int64_t iPC_UID;
	int8_t iSlotNum;
	int8_t iGender;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

struct sP_LS2CL_REP_SAVE_CHAR_NAME_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_CHAR_CREATE_SUCC {
	int16_t iLevel;
	sPCStyle sPC_Style;
	sPCStyle2 sPC_Style2;
	sOnItem sOn_Item;
};

struct sP_LS2CL_REP_CHAR_CREATE_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_CHAR_SELECT_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_SELECT_FAIL {
	int32_t iErrorCode;
};

#pragma pack(1)
struct sP_LS2CL_REP_CHAR_DELETE_SUCC {
	int8_t iSlotNum;
};

#pragma pack(4)
struct sP_LS2CL_REP_CHAR_DELETE_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_SHARD_SELECT_SUCC {
	uint8_t g_FE_ServerIP[16];
	int32_t g_FE_ServerPort;
	int64_t iEnterSerialKey;
};

struct sP_LS2CL_REP_SHARD_SELECT_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_VERSION_CHECK_SUCC {
	uint8_t UNUSED;
};

#pragma pack(4)
struct sP_LS2CL_REP_VERSION_CHECK_FAIL {
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_CHECK_NAME_LIST_SUCC {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
	long aNameCodeFlag[8];
};

struct sP_LS2CL_REP_CHECK_NAME_LIST_FAIL {
	int32_t iFNCode;
	int32_t iMNCode;
	int32_t iLNCode;
	int32_t iErrorCode;
};

struct sP_LS2CL_REP_PC_EXIT_DUPLICATE {
	int32_t iErrorCode;
};

struct sP_LS2CL_REQ_LIVE_CHECK {
	int32_t iTempValue;
};

struct sP_LS2CL_REP_CHANGE_CHAR_NAME_SUCC {
	int64_t iPC_UID;
	int8_t iSlotNum;
	char16_t szFirstName[9];
	char16_t szLastName[17];
};

struct sP_LS2CL_REP_CHANGE_CHAR_NAME_FAIL {
	int64_t iPC_UID;
	int8_t iSlotNum;
	int32_t iErrorCode;
};

#pragma pack(1)
struct sP_LS2CL_REP_SHARD_LIST_INFO_SUCC {
	uint8_t aShardConnectFlag[26];
};

#pragma pack(pop)