#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "NanoManager.hpp"
#include "PlayerManager.hpp"

void NanoManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_ACTIVE, nanoSummonHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_EQUIP, nanoEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_UNEQUIP, nanoUnEquipHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO, nanoGMGiveHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_TUNE, nanoSkillSetHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
}

void NanoManager::nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_EQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_EQUIP* nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_EQUIP_SUCC, resp);
    Player plr = PlayerManager::getPlayer(sock);

    if (nano->iNanoSlotNum > 2)
        return;

    resp.iNanoID = nano->iNanoID;
    resp.iNanoSlotNum = nano->iNanoSlotNum;


    // Update player
    plr.equippedNanos[nano->iNanoSlotNum] = nano->iNanoID;
    PlayerManager::updatePlayer(sock, plr);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_EQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC));
}

void NanoManager::nanoUnEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_UNEQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_UNEQUIP* nano = (sP_CL2FE_REQ_NANO_UNEQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_UNEQUIP_SUCC, resp);
    Player plr = PlayerManager::getPlayer(sock);

    if (nano->iNanoSlotNum > 2)
        return;

    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // update player
    plr.equippedNanos[nano->iNanoSlotNum] = 0;
    PlayerManager::updatePlayer(sock, plr);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_UNEQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_UNEQUIP_SUCC));
}

void NanoManager::nanoGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO))
        return; // ignore the malformed packet

    // Cmd: /nano <nanoId>
    sP_CL2FE_REQ_PC_GIVE_NANO* nano = (sP_CL2FE_REQ_PC_GIVE_NANO*)data->buf;
    Player plr = PlayerManager::getPlayer(sock);

    // Add nano to player
    addNano(sock, nano->iNanoID, 0);

    DEBUGLOG(
        std::cout << U16toU8(plr.PCStyle.szFirstName) << U16toU8(plr.PCStyle.szLastName) << " requested to add nano id: " << nano->iNanoID << std::endl;
    )
}

void NanoManager::nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_ACTIVE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_ACTIVE* pkt = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
    Player plr = PlayerManager::getPlayer(sock);

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = pkt->iNanoSlotNum;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));

    if (pkt->iNanoSlotNum > 2)
        return;

    int nanoId = plr.equippedNanos[pkt->iNanoSlotNum];

    if (nanoId > 36)
        return; // sanity check

    sNano nano = plr.Nanos[nanoId];

    // Send to other players
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);

    pkt1.iPC_ID = plr.iID;
    pkt1.Nano = nano;

    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));

    // update player
    plr.activeNano = nanoId;
    PlayerManager::updatePlayer(sock, plr);

    DEBUGLOG(
        std::cout << U16toU8(plr.PCStyle.szFirstName) << U16toU8(plr.PCStyle.szLastName) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_SKILL_USE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_SKILL_USE* skill = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;
    Player plr = PlayerManager::getPlayer(sock);

    // Send to client
    INITSTRUCT(sP_FE2CL_NANO_SKILL_USE_SUCC, resp);
    resp.iArg1 = skill->iArg1;
    resp.iArg2 = skill->iArg2;
    resp.iArg3 = skill->iArg3;
    resp.iBulletID = skill->iBulletID;
    resp.iTargetCnt = skill->iTargetCnt;
    resp.iPC_ID = plr.iID;
    resp.iNanoStamina = 150; // Hardcoded for now

    sock->sendPacket((void*)&resp, P_FE2CL_NANO_SKILL_USE_SUCC, sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));
}

void NanoManager::nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_TUNE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skill->iNanoID, skill->iTuneID);
}

#pragma region Helper methods
void NanoManager::addNano(CNSocket* sock, int16_t nanoId, int16_t slot) {
    if (nanoId > 36)
        return;

    Player plr = PlayerManager::getPlayer(sock);

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoId;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC, sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC));

    // Update player
    plr.Nanos[nanoId] = resp.Nano;
    PlayerManager::updatePlayer(sock, plr);
}

void NanoManager::setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId) {
    if (nanoId > 36)
        return;

    Player plr = PlayerManager::getPlayer(sock);
    sNano nano = plr.Nanos[nanoId];

    nano.iSkillID = skillId;
    plr.Nanos[nanoId] = nano;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_NANO_TUNE_SUCC, resp);
    resp.iNanoID = nanoId;
    resp.iSkillID = skillId;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));

    // Update the player
    PlayerManager::updatePlayer(sock, plr);

    DEBUGLOG(
        std::cout << U16toU8(plr.PCStyle.szFirstName) << U16toU8(plr.PCStyle.szLastName) << " set skill id " << skillId << " for nano: " << nanoId << std::endl;
    )

}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoId) {
    if (nanoId > 36)
        return;

    Player plr = PlayerManager::getPlayer(sock);
    sNano nano = plr.Nanos[nanoId];

    // 0 is reset
    nano.iSkillID = 0;
    plr.Nanos[nanoId] = nano;

    // Update the player
    PlayerManager::updatePlayer(sock, plr);
}
#pragma endregion
