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
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GIVE_NANO_SKILL, nanoSkillSetGMHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_NANO_SKILL_USE, nanoSkillUseHandler);
}

void NanoManager::nanoEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_EQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_EQUIP* nano = (sP_CL2FE_REQ_NANO_EQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_EQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
        return;

    resp.iNanoID = nano->iNanoID;
    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // Update player
    plr->equippedNanos[nano->iNanoSlotNum] = nano->iNanoID;

    // unsummon nano if replaced
    if (plr->activeNano == plr->equippedNanos[nano->iNanoSlotNum])
        summonNano(sock, -1);

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_EQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_EQUIP_SUCC));
}

void NanoManager::nanoUnEquipHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_UNEQUIP))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_UNEQUIP* nano = (sP_CL2FE_REQ_NANO_UNEQUIP*)data->buf;
    INITSTRUCT(sP_FE2CL_REP_NANO_UNEQUIP_SUCC, resp);
    Player *plr = PlayerManager::getPlayer(sock);

    // sanity check
    if (nano->iNanoSlotNum > 2 || nano->iNanoSlotNum < 0)
        return;

    resp.iNanoSlotNum = nano->iNanoSlotNum;

    // unsummon nano if removed
    if (plr->equippedNanos[nano->iNanoSlotNum] == plr->activeNano)
        summonNano(sock, -1);

    // update player
    plr->equippedNanos[nano->iNanoSlotNum] = 0;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_UNEQUIP_SUCC, sizeof(sP_FE2CL_REP_NANO_UNEQUIP_SUCC));
}

void NanoManager::nanoGMGiveHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO))
        return; // ignore the malformed packet

    // Cmd: /nano <nanoId>
    sP_CL2FE_REQ_PC_GIVE_NANO* nano = (sP_CL2FE_REQ_PC_GIVE_NANO*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // Add nano to player
    addNano(sock, nano->iNanoID, 0);

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to add nano id: " << nano->iNanoID << std::endl;
    )
}

void NanoManager::nanoSummonHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_ACTIVE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_ACTIVE* pkt = (sP_CL2FE_REQ_NANO_ACTIVE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    summonNano(sock, pkt->iNanoSlotNum);

    // Send to client
    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano slot: " << pkt->iNanoSlotNum << std::endl;
    )
}

void NanoManager::nanoSkillUseHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_SKILL_USE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_SKILL_USE* skill = (sP_CL2FE_REQ_NANO_SKILL_USE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // Send to client
    INITSTRUCT(sP_FE2CL_NANO_SKILL_USE_SUCC, resp);
    resp.iArg1 = skill->iArg1;
    resp.iArg2 = skill->iArg2;
    resp.iArg3 = skill->iArg3;
    resp.iBulletID = skill->iBulletID;
    resp.iTargetCnt = skill->iTargetCnt;
    resp.iPC_ID = plr->iID;
    resp.iNanoStamina = 150; // Hardcoded for now

    sock->sendPacket((void*)&resp, P_FE2CL_NANO_SKILL_USE_SUCC, sizeof(sP_FE2CL_NANO_SKILL_USE_SUCC));

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " requested to summon nano skill " << std::endl;
    )
}

void NanoManager::nanoSkillSetHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_NANO_TUNE))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skill = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skill->iNanoID, skill->iTuneID);
}

void NanoManager::nanoSkillSetGMHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_GIVE_NANO_SKILL))
        return; // malformed packet

    sP_CL2FE_REQ_NANO_TUNE* skillGM = (sP_CL2FE_REQ_NANO_TUNE*)data->buf;
    setNanoSkill(sock, skillGM->iNanoID, skillGM->iTuneID);
}

#pragma region Helper methods
void NanoManager::addNano(CNSocket* sock, int16_t nanoId, int16_t slot) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);

    int level = nanoId < plr->level ? plr->level : nanoId;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_PC_NANO_CREATE_SUCC, resp);
    resp.Nano.iID = nanoId;
    resp.Nano.iStamina = 150;
    resp.iQuestItemSlotNum = slot;
    resp.iPC_Level = level;

    // Update player
    plr->Nanos[nanoId] = resp.Nano;
    plr->level = level;
    plr->HP = 1000 * plr->level;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_NANO_CREATE_SUCC, sizeof(sP_FE2CL_REP_PC_NANO_CREATE_SUCC));

    /*
     * iPC_Level in NANO_CREATE_SUCC sets the player's level.
     * Other players must be notified of the change as well. Both P_FE2CL_REP_PC_NANO_CREATE and
     * P_FE2CL_REP_PC_CHANGE_LEVEL appear to play the same animation, but only the latter affects
     * the other player's displayed level.
     */

    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_LEVEL, resp2);

    resp2.iPC_ID = plr->iID;
    resp2.iPC_Level = level;

    // Update other players' perception of the player's level
    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&resp2, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
}

void NanoManager::summonNano(CNSocket *sock, int slot) {
    INITSTRUCT(sP_FE2CL_REP_NANO_ACTIVE_SUCC, resp);
    resp.iActiveNanoSlotNum = slot;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_ACTIVE_SUCC, sizeof(sP_FE2CL_REP_NANO_ACTIVE_SUCC));
    Player *plr = PlayerManager::getPlayer(sock);

    if (slot > 2 || slot < -1)
        return; // sanity check

    int nanoId = slot == -1 ? -1 : plr->equippedNanos[slot];

    if (nanoId > 36 || nanoId < -1)
        return; // sanity check

    // Send to other players
    INITSTRUCT(sP_FE2CL_NANO_ACTIVE, pkt1);

    pkt1.iPC_ID = plr->iID;
    if (nanoId == -1)
        memset(&pkt1.Nano, 0, sizeof(pkt1.Nano));
    else
        pkt1.Nano = plr->Nanos[nanoId];

    for (CNSocket* s : PlayerManager::players[sock].viewable)
        s->sendPacket((void*)&pkt1, P_FE2CL_NANO_ACTIVE, sizeof(sP_FE2CL_NANO_ACTIVE));

    // update player
    plr->activeNano = nanoId;
}

void NanoManager::setNanoSkill(CNSocket* sock, int16_t nanoId, int16_t skillId) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);
    sNano nano = plr->Nanos[nanoId];

    nano.iSkillID = skillId;
    plr->Nanos[nanoId] = nano;

    // Send to client
    INITSTRUCT(sP_FE2CL_REP_NANO_TUNE_SUCC, resp);
    resp.iNanoID = nanoId;
    resp.iSkillID = skillId;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_NANO_TUNE_SUCC, sizeof(sP_FE2CL_REP_NANO_TUNE_SUCC));

    DEBUGLOG(
        std::cout << U16toU8(plr->PCStyle.szFirstName) << U16toU8(plr->PCStyle.szLastName) << " set skill id " << skillId << " for nano: " << nanoId << std::endl;
    )
}

void NanoManager::resetNanoSkill(CNSocket* sock, int16_t nanoId) {
    if (nanoId > 36)
        return;

    Player *plr = PlayerManager::getPlayer(sock);
    sNano nano = plr->Nanos[nanoId];

    // 0 is reset
    nano.iSkillID = 0;
    plr->Nanos[nanoId] = nano;
}
#pragma endregion
