#include "Entities.hpp"

#include "NPCManager.hpp"
#include "PlayerManager.hpp"

#include <assert.h>

static_assert(std::is_standard_layout<EntityRef>::value);
static_assert(std::is_trivially_copyable<EntityRef>::value);

EntityRef::EntityRef(CNSocket *s) {
    kind = EntityKind::PLAYER;
    sock = s;
}

EntityRef::EntityRef(int32_t i) {
    id = i;

    kind = EntityKind::INVALID;
    if (NPCManager::NPCs.find(id) != NPCManager::NPCs.end())
        kind = NPCManager::NPCs[id]->kind;
}

bool EntityRef::isValid() const {
    if (kind == EntityKind::PLAYER)
        return PlayerManager::players.find(sock) != PlayerManager::players.end();

    return NPCManager::NPCs.find(id) != NPCManager::NPCs.end();
}

Entity *EntityRef::getEntity() const {
    assert(isValid());

    if (kind == EntityKind::PLAYER)
        return PlayerManager::getPlayer(sock);

    return NPCManager::NPCs[id];
}

sNPCAppearanceData BaseNPC::getAppearanceData() {
    sNPCAppearanceData data = {};
    data.iAngle = angle;
    data.iBarkerType = 0; // unused?
    data.iConditionBitFlag = 0;
    data.iHP = hp;
    data.iNPCType = type;
    data.iNPC_ID = id;
    data.iX = x;
    data.iY = y;
    data.iZ = z;
    return data;
}

sNPCAppearanceData CombatNPC::getAppearanceData() {
    sNPCAppearanceData data = BaseNPC::getAppearanceData();
    data.iConditionBitFlag = getCompositeCondition();
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
    pkt.AppearanceData = getTransportationAppearanceData();
    sock->sendPacket(pkt, P_FE2CL_TRANSPORTATION_ENTER);
}

void Egg::enterIntoViewOf(CNSocket *sock) {
    INITSTRUCT(sP_FE2CL_SHINY_ENTER, pkt);

    // TODO: Potentially decouple this from BaseNPC?
    pkt.ShinyAppearanceData = getShinyAppearanceData();
    sock->sendPacket(pkt, P_FE2CL_SHINY_ENTER);
}

sTransportationAppearanceData Bus::getTransportationAppearanceData() {
    return sTransportationAppearanceData {
        3, id, type,
        x, y, z
    };
}

sShinyAppearanceData Egg::getShinyAppearanceData() {
    return sShinyAppearanceData {
        id, type, 0, // client doesn't care about map num
        x, y, z
    };
}

sNano* Player::getActiveNano() {
    return &Nanos[activeNano];
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

bool Player::hasQuestBoost() const {
    const sItemBase& booster = Equip[10];
    return booster.iID == 153 && booster.iOpt > 0;
}

bool Player::hasHunterBoost() const {
    const sItemBase& booster = Equip[11];
    return booster.iID == 154 && booster.iOpt > 0;
}

bool Player::hasRacerBoost() const {
    const sItemBase& booster = Equip[9];
    return booster.iID == 155 && booster.iOpt > 0;
}

bool Player::hasSuperBoost() const {
    return Player::hasQuestBoost() && Player::hasHunterBoost() && Player::hasRacerBoost();
}

static int32_t getCap(CappedValueType type) {
    switch (type) {
    case CappedValueType::TAROS:
        return PC_CANDY_MAX;
    case CappedValueType::FUSIONMATTER:
        return PC_FUSIONMATTER_MAX;
    case CappedValueType::BATTERY_W:
        return PC_BATTERY_MAX;
    case CappedValueType::BATTERY_N:
        return PC_BATTERY_MAX;
    case CappedValueType::TAROS_IN_TRADE:
        return PC_CANDY_MAX;
    default:
        return INT32_MAX;
    }
}

static int32_t *getCappedValue(Player *player, CappedValueType type) {
    switch (type) {
    case CappedValueType::TAROS:
        return &player->money;
    case CappedValueType::FUSIONMATTER:
        return &player->fusionmatter;
    case CappedValueType::BATTERY_W:
        return &player->batteryW;
    case CappedValueType::BATTERY_N:
        return &player->batteryN;
    case CappedValueType::TAROS_IN_TRADE:
        return &player->moneyInTrade;
    default:
        return nullptr;
    }
}

void Player::addCapped(CappedValueType type, int32_t diff) {
    if (diff <= 0)
        return;

    int32_t max = getCap(type);
    int32_t *value = getCappedValue(this, type);

    if (value == nullptr)
        return;

    if (diff > max)
        diff = max;

    if (*value + diff > max)
        *value = max;
    else
        *value += diff;
}

void Player::subtractCapped(CappedValueType type, int32_t diff) {
    if (diff <= 0)
        return;

    int32_t max = getCap(type);
    int32_t *value = getCappedValue(this, type);

    if (value == nullptr)
        return;

    if (diff > max)
        diff = max;

    if (*value - diff < 0)
        *value = 0;
    else
        *value -= diff;
}

void Player::setCapped(CappedValueType type, int32_t value) {
    int32_t max = getCap(type);
    int32_t *valToSet = getCappedValue(this, type);

    if (valToSet == nullptr)
        return;

    if (value < 0)
        value = 0;
    else if (value > max)
        value = max;

    *valToSet = value;
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
