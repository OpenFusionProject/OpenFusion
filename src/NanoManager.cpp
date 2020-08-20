#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"

void NanoManager::init() {
	REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
}

void NanoManager::nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
	if (data->size != sizeof(sP_CL2FE_REQ_NANO_ACTIVE))
        return; // malformed packet

	sP_CL2FE_REQ_NANO_ACTIVE* nano = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
	PlayerView plr = PlayerManager::players[sock];

	// Send to client
	sP_FE2CL_REP_NANO_ACTIVE_SUCC* resp = (sP_FE2CL_REP_NANO_ACTIVE_SUCC*)xmalloc(sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));
	resp->iActiveNanoSlotNum = nano->iNanoSlotNum;
	sock->sendPacket(new CNPacketData((void*)resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC), sock->getFEKey()));

	std::cout << U16toU8(plr.plr.PCStyle.szFirstName) << U16toU8(plr.plr.PCStyle.szLastName) << " requested to summon nano slot: " << nano->iNanoSlotNum << std::endl;
}