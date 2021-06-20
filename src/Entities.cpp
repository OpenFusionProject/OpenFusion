#include "core/Core.hpp"
#include "Entities.hpp"
#include "Chunking.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "Eggs.hpp"
#include "MobAI.hpp"

#include <type_traits>

static_assert(std::is_standard_layout<EntityRef>::value);
static_assert(std::is_trivially_copyable<EntityRef>::value);

EntityRef::EntityRef(CNSocket *s) {
    type = EntityType::PLAYER;
    sock = s;
}

EntityRef::EntityRef(int32_t i) {
    id = i;

    assert(NPCManager::NPCs.find(id) != NPCManager::NPCs.end());
    type = NPCManager::NPCs[id]->kind;
}

bool EntityRef::isValid() const {
    if (type == EntityType::PLAYER)
        return PlayerManager::players.find(sock) != PlayerManager::players.end();

    return NPCManager::NPCs.find(id) != NPCManager::NPCs.end();
}

Entity *EntityRef::getEntity() const {
    assert(isValid());

    if (type == EntityType::PLAYER)
        return PlayerManager::getPlayer(sock);

    return NPCManager::NPCs[id];
}

sNPCAppearanceData BaseNPC::getAppearanceData() {
    sNPCAppearanceData data = {};
    data.iAngle = angle;
    data.iBarkerType = barkerType;
    data.iConditionBitFlag = cbf;
    data.iHP = hp;
    data.iNPCType = type;
    data.iNPC_ID = id;
    data.iX = x;
    data.iY = y;
    data.iZ = z;
    return data;
}

/*
 * Entity coming into view.
 */
void BaseNPC::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_NPC_ENTER, pkt);
    pkt.NPCAppearanceData = getAppearanceData();
    sock->sendPacket(pkt, P_FE2CL_NPC_ENTER);
}

void Bus::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, pkt);

    // TODO: Potentially decouple this from BaseNPC?
    pkt.AppearanceData = {
        3, id, type,
        x, y, z
    };

    sock->sendPacket(pkt, P_FE2CL_TRANSPORTATION_ENTER);
}

void Egg::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_SHINY_ENTER, pkt);

    // TODO: Potentially decouple this from BaseNPC?
    pkt.ShinyAppearanceData = {
        id, type, 0, // client doesn't care about map num
        x, y, z
    };

    sock->sendPacket(pkt, P_FE2CL_SHINY_ENTER);
}

sPCAppearanceData Player::getAppearanceData() {
    sPCAppearanceData data = {};
    data.iID = iID;
    data.iHP = HP;
    data.iLv = level;
    data.iX = x;
    data.iY = y;
    data.iZ = z;
    data.iAngle = angle;
    data.PCStyle = PCStyle;
    data.Nano = Nanos[activeNano];
    data.iPCState = iPCState;
    data.iSpecialState = iSpecialState;
    memcpy(data.ItemEquip, Equip, sizeof(sItemBase) * AEQUIP_COUNT);
    return data;
}

// TODO: this is less effiecient than it was, because of memset()
void Player::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_PC_NEW, pkt);
    pkt.PCAppearanceData = getAppearanceData();
    sock->sendPacket(pkt, P_FE2CL_PC_NEW);
}

/*
 * Entity leaving view.
 */
void BaseNPC::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);
    pkt.iNPC_ID = id;
    sock->sendPacket(pkt, P_FE2CL_NPC_EXIT);
}

void Bus::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, pkt);
    pkt.eTT = 3;
    pkt.iT_ID = id;
    sock->sendPacket(pkt, P_FE2CL_TRANSPORTATION_EXIT);
}

void Egg::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_SHINY_EXIT, pkt);
    pkt.iShinyID = id;
    sock->sendPacket(pkt, P_FE2CL_SHINY_EXIT);
}

void Player::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_PC_EXIT, pkt);
    pkt.iID = iID;
    sock->sendPacket(pkt, P_FE2CL_PC_EXIT);
}
