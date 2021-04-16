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
    type = NPCManager::NPCs[id]->type;
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

/*
 * Entity coming into view.
 */
void BaseNPC::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_NPC_ENTER, pkt);
    pkt.NPCAppearanceData = appearanceData;
    pkt.NPCAppearanceData.iX = x;
    pkt.NPCAppearanceData.iY = y;
    pkt.NPCAppearanceData.iZ = z;
    sock->sendPacket(pkt, P_FE2CL_NPC_ENTER);
}

void Bus::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, pkt);

    // TODO: Potentially decouple this from BaseNPC?
    pkt.AppearanceData = {
        3, appearanceData.iNPC_ID, appearanceData.iNPCType,
        x, y, z
    };

    sock->sendPacket(pkt, P_FE2CL_TRANSPORTATION_ENTER);
}

void Egg::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_SHINY_ENTER, pkt);

    Eggs::npcDataToEggData(x, y, z, &appearanceData, &pkt.ShinyAppearanceData);

    sock->sendPacket(pkt, P_FE2CL_SHINY_ENTER);
}
    
// TODO: this is less effiecient than it was, because of memset()
void Player::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_PC_NEW, pkt);

    pkt.PCAppearanceData.iID = iID;
    pkt.PCAppearanceData.iHP = HP;
    pkt.PCAppearanceData.iLv = level;
    pkt.PCAppearanceData.iX = x;
    pkt.PCAppearanceData.iY = y;
    pkt.PCAppearanceData.iZ = z;
    pkt.PCAppearanceData.iAngle = angle;
    pkt.PCAppearanceData.PCStyle = PCStyle;
    pkt.PCAppearanceData.Nano = Nanos[activeNano];
    pkt.PCAppearanceData.iPCState = iPCState;
    pkt.PCAppearanceData.iSpecialState = iSpecialState;
    memcpy(pkt.PCAppearanceData.ItemEquip, Equip, sizeof(sItemBase) * AEQUIP_COUNT);

    sock->sendPacket(pkt, P_FE2CL_PC_NEW);
}

/*
 * Entity leaving view.
 */
void BaseNPC::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_NPC_EXIT, pkt);
    pkt.iNPC_ID = appearanceData.iNPC_ID;
    sock->sendPacket(pkt, P_FE2CL_NPC_EXIT);
}

void Bus::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, pkt);
    pkt.eTT = 3;
    pkt.iT_ID = appearanceData.iNPC_ID;
    sock->sendPacket(pkt, P_FE2CL_TRANSPORTATION_EXIT);
}

void Egg::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_SHINY_EXIT, pkt);
    pkt.iShinyID = appearanceData.iNPC_ID;
    sock->sendPacket(pkt, P_FE2CL_SHINY_EXIT);
}

void Player::disappearFromViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_PC_EXIT, pkt);
    pkt.iID = iID;
    sock->sendPacket(pkt, P_FE2CL_PC_EXIT);
}
