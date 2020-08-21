#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"

void NanoManager::init() {
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
}

void NanoManager::nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
	sP_CL2FE_REQ_NANO_EQUIP* nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
	sP_FE2CL_REP_NANO_EQUIP_SUCC* resp = (sP_FE2CL_REP_NANO_EQUIP_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC));
	resp->iNanoID = nano->iNanoID;
	resp->iNanoSlotNum = nano->iNanoSlotNum;

	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_NANO_EQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC), sock->getFEKey()));
}

void NanoManager::nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
	sP_CL2FE_REQ_NANO_ACTIVE* nano = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
	PlayerView plr = PlayerManager::players[sock];

	// Send to client
	sP_FE2CL_REP_NANO_ACTIVE_SUCC* resp = (sP_FE2CL_REP_NANO_ACTIVE_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));
	resp->iActiveNanoSlotNum = nano->iNanoSlotNum;
	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC), sock->getFEKey()));

	std::cout << U16toU8(plr.plr.PCStyle.szFirstName) << U16toU8(plr.plr.PCStyle.szLastName) << " requested to summon nano slot: " << nano->iNanoSlotNum << std::endl;
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
	sP_CL2FE_REQ_NANO_SKILL_USE* skill = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;
	PlayerView plr = PlayerManager::players[sock];

	// Send to client
	sP_FE2CL_NANO_SKILL_USE_SUCC* resp = (sP_FE2CL_NANO_SKILL_USE_SUCC*)xmalloc(sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));
	resp->iArg1 = skill->iArg1;
	resp->iArg2 = skill->iArg2;
	resp->iArg3 = skill->iArg3;
	resp->iBulletID = skill->iBulletID;
	resp->iTargetCnt = skill->iTargetCnt;
	resp->iPC_ID = plr.plr.iID;
	resp->iNanoStamina = 150; // Hardcoded for now

	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_NANO_SKILL_USE_SUCC, sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC), sock->getFEKey()));

	std::cout << U16toU8(plr.plr.PCStyle.szFirstName) << U16toU8(plr.plr.PCStyle.szLastName) << " requested to summon nano skill " << std::endl;
}

void NanoManager::nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
	sP_CL2FE_REQ_NANO_TUNE* skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
	setNanoSkill(sock, skill->iNanoID, skill->iTuneID);
}

void NanoManager::addNano(CNSocket* sock, int16_t nanoId, int16_t slot) {
	Player plr = PlayerManager::getPlayer(sock);

	// Send to client
	sP_FE2CL_REP_PC_NANO_CREATE_SUCC* resp = new sP_FE2CL_REP_PC_NANO_CREATE_SUCC();
	resp->Nano.iID = nanoId;
	resp->Nano.iStamina = 150;
	resp->iQuestItemSlotNum = slot;

	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC, sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC), sock->getFEKey()));

	// Update player
	plr.Nanos[nanoId] = resp->Nano;
	PlayerManager::updatePlayer(sock, plr);
}

void NanoManager::setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId) {
	Player plr = PlayerManager::getPlayer(sock);
	sNano nano = plr.Nanos[nanoId];

	nano.iSkillID = skillId;
	plr.Nanos[nanoId] = nano;

	// Send to client
	sP_FE2CL_REP_NANO_TUNE_SUCC* resp = (sP_FE2CL_REP_NANO_TUNE_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));
	resp->iNanoID = nanoId;
	resp->iSkillID = skillId;

	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC), sock->getFEKey()));

	std::cout << U16toU8(plr.PCStyle.szFirstName) << U16toU8(plr.PCStyle.szLastName) << " set skill id " << skillId << " for nano: " << nanoId << std::endl;

	// Update the player
	PlayerManager::updatePlayer(sock, plr);
}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoId) {
	Player plr = PlayerManager::getPlayer(sock);
	sNano nano = plr.Nanos[nanoId];

	nano.iSkillID = 0;
	plr.Nanos[nanoId] = nano;

	// Update the player
	PlayerManager::updatePlayer(sock, plr);
}