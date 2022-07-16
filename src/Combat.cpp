#include "Combat.hpp"
#include "PlayerManager.hpp"
#include "Nanos.hpp"
#include "NPCManager.hpp"
#include "Items.hpp"
#include "Missions.hpp"
#include "Groups.hpp"
#include "Transport.hpp"
#include "Racing.hpp"
#include "Abilities.hpp"
#include "Rand.hpp"

#include <assert.h>

using namespace Combat;

/// Player Id -> Bullet Id -> Bullet
std::map<int32_t, std::map<int8_t, Bullet>> Combat::Bullets;

int Player::takeDamage(EntityRef src, int amt) {
    HP -= amt;
    return amt;
}

void Player::heal(EntityRef src, int amt) {
    // stubbed
}

bool Player::isAlive() {
    return HP > 0;
}

int Player::getCurrentHP() {
    return HP;
}

int32_t Player::getID() {
    return iID;
}

void Player::step(time_t currTime) {
    // no-op
}

int CombatNPC::takeDamage(EntityRef src, int amt) {

    hp -= amt;
    if (hp <= 0)
        transition(AIState::DEAD, src);

    return amt;
}

void CombatNPC::heal(EntityRef src, int amt) {
    // stubbed
}

bool CombatNPC::isAlive() {
    return hp > 0;
}

int CombatNPC::getCurrentHP() {
    return hp;
}

int32_t CombatNPC::getID() {
    return id;
}

void CombatNPC::step(time_t currTime) {
    
    if(stateHandlers.find(state) != stateHandlers.end())
        stateHandlers[state](this, currTime);
    else {
        std::cout << "[WARN] State " << (int)state << " has no handler; going inactive" << std::endl;
        transition(AIState::INACTIVE, id);
    }
}

void CombatNPC::transition(AIState newState, EntityRef src) {

    state = newState;
    if (transitionHandlers.find(newState) != transitionHandlers.end())
        transitionHandlers[newState](this, src);
    else {
        std::cout << "[WARN] Transition to " << (int)state << " has no handler; going inactive" << std::endl;
        transition(AIState::INACTIVE, id);
    }
    /* TODO: fire any triggered events
    for (NPCEvent& event : NPCManager::NPCEvents)
        if (event.trigger == ON_KILLED && event.npcType == type)
            event.handler(src, this);
    */
}

static std::pair<int,int> getDamage(int attackPower, int defensePower, bool shouldCrit,
                                         bool batteryBoost, int attackerStyle,
                                         int defenderStyle, int difficulty) {
    std::pair<int,int> ret = {0, 1};
    if (attackPower + defensePower * 2 == 0)
        return ret;

    // base calculation
    int damage = attackPower * attackPower / (attackPower + defensePower);
    damage = std::max(10 + attackPower / 10, damage - (defensePower - attackPower / 6) * difficulty / 100);
    damage = damage * (Rand::rand(40) + 80) / 100;

    // Adaptium/Blastons/Cosmix
    if (attackerStyle != -1 && defenderStyle != -1 && attackerStyle != defenderStyle) {
        if (attackerStyle - defenderStyle == 2)
            defenderStyle += 3;
        if (defenderStyle - attackerStyle == 2)
            defenderStyle -= 3;
        if (attackerStyle < defenderStyle)
            damage = damage * 5 / 4;
        else
            damage = damage * 4 / 5;
    }

    // weapon boosts
    if (batteryBoost)
        damage = damage * 5 / 4;

    ret.first = damage;
    ret.second = 1;

    if (shouldCrit && Rand::rand(20) == 0) {
        ret.first *= 2; // critical hit
        ret.second = 2;
    }

    return ret;
}

static bool checkRapidFire(CNSocket *sock, int targetCount) {
    Player *plr = PlayerManager::getPlayer(sock);
    time_t currTime = getTime();

    if (currTime - plr->lastShot < plr->fireRate * 80)
        plr->suspicionRating += plr->fireRate * 100 + plr->lastShot - currTime; // gain suspicion for rapid firing
    else if (currTime - plr->lastShot < plr->fireRate * 180 && plr->suspicionRating > 0)
        plr->suspicionRating += plr->fireRate * 100 + plr->lastShot - currTime; // lose suspicion for delayed firing

    plr->lastShot = currTime;

    // 3+ targets should never be possible
    if (targetCount > 3)
        plr->suspicionRating += 10001;

    // kill the socket when the player is too suspicious
    if (plr->suspicionRating > 10000) {
        sock->kill();
        CNShardServer::_killConnection(sock);
        return true;
    }

    return false;
}

static void pcAttackNpcs(CNSocket *sock, CNPacketData *data) {
    auto pkt = (sP_CL2FE_REQ_PC_ATTACK_NPCs*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);
    auto targets = (int32_t*)data->trailers;

    // kick the player if firing too rapidly
    if (settings::ANTICHEAT && checkRapidFire(sock, pkt->iNPCCnt))
        return;

    /*
     * IMPORTANT: This validates memory safety in addition to preventing
     * ordinary cheating. If the client sends a very large number of trailing
     * values, it could overflow the *response* buffer, which isn't otherwise
     * being validated anymore.
     */
    if (pkt->iNPCCnt > 3) {
        std::cout << "[WARN] Player tried to attack more than 3 NPCs at once" << std::endl;
        return;
    }

    INITVARPACKET(respbuf, sP_FE2CL_PC_ATTACK_NPCs_SUCC, resp, sAttackResult, respdata);

    resp->iNPCCnt = pkt->iNPCCnt;

    for (int i = 0; i < data->trCnt; i++) {
        if (NPCManager::NPCs.find(targets[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] pcAttackNpcs: NPC ID not found" << std::endl;
            return;
        }


        BaseNPC* npc = NPCManager::NPCs[targets[i]];
        if (npc->kind != EntityKind::MOB) {
            std::cout << "[WARN] pcAttackNpcs: NPC is not a mob" << std::endl;
            return;
        }

        Mob* mob = (Mob*)npc;

        std::pair<int,int> damage;

        if (pkt->iNPCCnt > 1)
            damage.first = plr->groupDamage;
        else
            damage.first = plr->pointDamage;

        int difficulty = (int)mob->data["m_iNpcLevel"];
        damage = getDamage(damage.first, (int)mob->data["m_iProtection"], true, (plr->batteryW > 6 + difficulty),
            Nanos::nanoStyle(plr->activeNano), (int)mob->data["m_iNpcStyle"], difficulty);

        if (plr->batteryW >= 6 + difficulty)
            plr->batteryW -= 6 + difficulty;
        else
            plr->batteryW = 0;

        damage.first = mob->takeDamage(sock, damage.first);

        respdata[i].iID = mob->id;
        respdata[i].iDamage = damage.first;
        respdata[i].iHP = mob->hp;
        respdata[i].iHitFlag = damage.second; // hitscan, not a rocket or a grenade
    }

    resp->iBatteryW = plr->batteryW;
    sock->sendPacket(respbuf, P_FE2CL_PC_ATTACK_NPCs_SUCC);

    // a bit of a hack: these are the same size, so we can reuse the response packet
    assert(sizeof(sP_FE2CL_PC_ATTACK_NPCs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_NPCs));
    auto *resp1 = (sP_FE2CL_PC_ATTACK_NPCs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, respbuf, P_FE2CL_PC_ATTACK_NPCs);
}

void Combat::npcAttackPc(Mob *mob, time_t currTime) {
    Player *plr = PlayerManager::getPlayer(mob->target);

    INITVARPACKET(respbuf, sP_FE2CL_NPC_ATTACK_PCs, pkt, sAttackResult, atk);

    auto damage = getDamage(450 + (int)mob->data["m_iPower"], plr->defense, true, false, -1, -1, 0);

    if (!(plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE))
        plr->HP -= damage.first;

    pkt->iNPC_ID = mob->id;
    pkt->iPCCnt = 1;

    atk->iID = plr->iID;
    atk->iDamage = damage.first;
    atk->iHP = plr->HP;
    atk->iHitFlag = damage.second;

    mob->target->sendPacket(respbuf, P_FE2CL_NPC_ATTACK_PCs);
    PlayerManager::sendToViewable(mob->target, respbuf, P_FE2CL_NPC_ATTACK_PCs);

    if (plr->HP <= 0) {
        if (!MobAI::aggroCheck(mob, getTime()))
            mob->transition(AIState::RETREAT, mob->target);
    }
}

/*
 * When a group of players is doing missions together, we want them to all get
 * quest items at the same time, but we don't want the odds of quest item
 * drops from different missions to be linked together. That's why we use a
 * single RNG roll per mission task, and every group member shares that same
 * set of rolls.
 */
void Combat::genQItemRolls(std::vector<Player*> players, std::map<int, int>& rolls) {
    for (int i = 0; i < players.size(); i++) {

        Player* member = players[i];
        for (int j = 0; j < ACTIVE_MISSION_COUNT; j++)
            if (member->tasks[j] != 0)
                rolls[member->tasks[j]] = Rand::rand();
    }
}

static void combatBegin(CNSocket *sock, CNPacketData *data) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->inCombat = true;

    // HACK: make sure the player has the right weapon out for combat
    INITSTRUCT(sP_FE2CL_PC_EQUIP_CHANGE, resp);

    resp.iPC_ID = plr->iID;
    resp.iEquipSlotNum = 0;
    resp.EquipSlotItem = plr->Equip[0];

    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_PC_EQUIP_CHANGE, sizeof(sP_FE2CL_PC_EQUIP_CHANGE));
}

static void combatEnd(CNSocket *sock, CNPacketData *data) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->inCombat = false;
    plr->healCooldown = 4000;
}

static void dotDamageOnOff(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_DOT_DAMAGE_ONOFF *pkt = (sP_CL2FE_DOT_DAMAGE_ONOFF*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    if ((plr->iConditionBitFlag & CSB_BIT_INFECTION) != (bool)pkt->iFlag)
        plr->iConditionBitFlag ^= CSB_BIT_INFECTION;

    INITSTRUCT(sP_FE2CL_PC_BUFF_UPDATE, pkt1);

    pkt1.eCSTB = ECSB_INFECTION; // eCharStatusTimeBuffID
    pkt1.eTBU = 1; // eTimeBuffUpdate
    pkt1.eTBT = 0; // eTimeBuffType 1 means nano
    pkt1.iConditionBitFlag = plr->iConditionBitFlag;

    sock->sendPacket((void*)&pkt1, P_FE2CL_PC_BUFF_UPDATE, sizeof(sP_FE2CL_PC_BUFF_UPDATE));
}

static void dealGooDamage(CNSocket *sock, int amount) {
    size_t resplen = sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK) + sizeof(sSkillResult_DotDamage);
    assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    Player *plr = PlayerManager::getPlayer(sock);

    memset(respbuf, 0, resplen);

    sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK *pkt = (sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK*)respbuf;
    sSkillResult_DotDamage *dmg = (sSkillResult_DotDamage*)(respbuf + sizeof(sP_FE2CL_CHAR_TIME_BUFF_TIME_TICK));

    if (plr->iConditionBitFlag & CSB_BIT_PROTECT_INFECTION) {
        amount = -2; // -2 is the magic number for "Protected" to appear as the damage number
        dmg->bProtected = 1;

        // eggs allow protection without nanos
        if (plr->activeNano != -1 && (plr->iSelfConditionBitFlag & CSB_BIT_PROTECT_INFECTION))
            plr->Nanos[plr->activeNano].iStamina -= 3;
    } else {
        plr->HP -= amount;
    }

    if (plr->activeNano != 0) {
        dmg->iStamina = plr->Nanos[plr->activeNano].iStamina;

        if (plr->Nanos[plr->activeNano].iStamina <= 0) {
            dmg->bNanoDeactive = 1;
            plr->Nanos[plr->activeNano].iStamina = 0;
            Nanos::summonNano(PlayerManager::getSockFromID(plr->iID), -1, true);
        }
    }

    pkt->iID = plr->iID;
    pkt->eCT = 1; // player
    pkt->iTB_ID = ECSB_INFECTION; // sSkillResult_DotDamage

    dmg->eCT = 1;
    dmg->iID = plr->iID;
    dmg->iDamage = amount;
    dmg->iHP = plr->HP;
    dmg->iConditionBitFlag = plr->iConditionBitFlag;

    sock->sendPacket((void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
    PlayerManager::sendToViewable(sock, (void*)&respbuf, P_FE2CL_CHAR_TIME_BUFF_TIME_TICK, resplen);
}

static void pcAttackChars(CNSocket *sock, CNPacketData *data) {
    sP_CL2FE_REQ_PC_ATTACK_CHARs* pkt = (sP_CL2FE_REQ_PC_ATTACK_CHARs*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    // only GMs can use this variant
    if (plr->accountLevel > 30)
        return;

    // Unlike the attack mob packet, attacking players packet has an 8-byte trail (Instead of 4 bytes).
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs), pkt->iTargetCnt, sizeof(sGM_PVPTarget), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_PC_ATTACK_CHARs packet size\n";
        return;
    }

    sGM_PVPTarget* pktdata = (sGM_PVPTarget*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_PC_ATTACK_CHARs));

    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC), pkt->iTargetCnt, sizeof(sAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_ATTACK_CHARs_SUCC packet size\n";
        return;
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) + pkt->iTargetCnt * sizeof(sAttackResult);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_ATTACK_CHARs_SUCC *resp = (sP_FE2CL_PC_ATTACK_CHARs_SUCC*)respbuf;
    sAttackResult *respdata = (sAttackResult*)(respbuf+sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC));

    resp->iTargetCnt = pkt->iTargetCnt;

    for (int i = 0; i < pkt->iTargetCnt; i++) {

        ICombatant* target = nullptr;
        std::pair<int, int> damage;

        if (pkt->iTargetCnt > 1)
            damage.first = plr->groupDamage;
        else
            damage.first = plr->pointDamage;

        if (pktdata[i].eCT == 1) { // eCT == 1; attack player

            for (auto& pair : PlayerManager::players) {
                if (pair.second->iID == pktdata[i].iID) {
                    target = pair.second;
                    break;
                }
            }

            if (target == nullptr) {
                // you shall not pass
                std::cout << "[WARN] pcAttackChars: player ID not found" << std::endl;
                return;
            }

            damage = getDamage(damage.first, ((Player*)target)->defense, true, (plr->batteryW > 6 + plr->level), -1, -1, 0);

        } else { // eCT == 4; attack mob

            if (NPCManager::NPCs.find(pktdata[i].iID) == NPCManager::NPCs.end()) {
                // not sure how to best handle this
                std::cout << "[WARN] pcAttackChars: NPC ID not found" << std::endl;
                return;
            }

            BaseNPC* npc = NPCManager::NPCs[pktdata[i].iID];
            if (npc->kind != EntityKind::MOB) {
                std::cout << "[WARN] pcAttackChars: NPC is not a mob" << std::endl;
                return;
            }

            Mob* mob = (Mob*)npc;
            target = mob;
            int difficulty = (int)mob->data["m_iNpcLevel"];
            damage = getDamage(damage.first, (int)mob->data["m_iProtection"], true, (plr->batteryW > 6 + difficulty),
                Nanos::nanoStyle(plr->activeNano), (int)mob->data["m_iNpcStyle"], difficulty);
        }

        if (plr->batteryW >= 6 + plr->level)
            plr->batteryW -= 6 + plr->level;
        else
            plr->batteryW = 0;

        damage.first = target->takeDamage(sock, damage.first);

        respdata[i].eCT = pktdata[i].eCT;
        respdata[i].iID = target->getID();
        respdata[i].iDamage = damage.first;
        respdata[i].iHP = target->getCurrentHP();
        respdata[i].iHitFlag = damage.second; // hitscan, not a rocket or a grenade
    }

    sock->sendPacket((void*)respbuf, P_FE2CL_PC_ATTACK_CHARs_SUCC, resplen);

    // a bit of a hack: these are the same size, so we can reuse the response packet
    assert(sizeof(sP_FE2CL_PC_ATTACK_CHARs_SUCC) == sizeof(sP_FE2CL_PC_ATTACK_CHARs));
    sP_FE2CL_PC_ATTACK_CHARs *resp1 = (sP_FE2CL_PC_ATTACK_CHARs*)respbuf;

    resp1->iPC_ID = plr->iID;

    // send to other players
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_ATTACK_CHARs, resplen);
}

static int8_t addBullet(Player* plr, bool isGrenade) {

    int8_t findId = 0;
    if (Bullets.find(plr->iID) != Bullets.end()) {
        // find first free id
        for (; findId < 127; findId++)
            if (Bullets[plr->iID].find(findId) == Bullets[plr->iID].end())
                break;
    }

    // sanity check
    if (findId == 127) {
        std::cout << "[WARN] Player has more than 127 active projectiles?!" << std::endl;
        findId = 0;
    }

    Bullet toAdd;
    toAdd.pointDamage = plr->pointDamage;
    toAdd.groupDamage = plr->groupDamage;
    // for grenade we need to send 1, for rocket - weapon id
    toAdd.bulletType = isGrenade ? 1 : plr->Equip[0].iID;

    // temp solution Jade fix plz
    toAdd.weaponBoost = plr->batteryW > 0;
    if (toAdd.weaponBoost) {
        int boostCost = Rand::rand(11) + 20;
        plr->batteryW = boostCost > plr->batteryW ? 0 : plr->batteryW - boostCost;
    }

    Bullets[plr->iID][findId] = toAdd;
    return findId;
}

static void grenadeFire(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE* grenade = (sP_CL2FE_REQ_PC_GRENADE_STYLE_FIRE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    INITSTRUCT(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC, resp);
    resp.iToX = grenade->iToX;
    resp.iToY = grenade->iToY;
    resp.iToZ = grenade->iToZ;

    resp.iBulletID = addBullet(plr, true);
    resp.iBatteryW = plr->batteryW;

    // 1 means grenade
    resp.Bullet.iID = 1;
    sock->sendPacket(&resp, P_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC, sizeof(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC));

    // send packet to nearby players
    INITSTRUCT(sP_FE2CL_PC_GRENADE_STYLE_FIRE, toOthers);
    toOthers.iPC_ID = plr->iID;
    toOthers.iToX = resp.iToX;
    toOthers.iToY = resp.iToY;
    toOthers.iToZ = resp.iToZ;
    toOthers.iBulletID = resp.iBulletID;
    toOthers.Bullet.iID = resp.Bullet.iID;

    PlayerManager::sendToViewable(sock, &toOthers, P_FE2CL_PC_GRENADE_STYLE_FIRE, sizeof(sP_FE2CL_PC_GRENADE_STYLE_FIRE));
}

static void rocketFire(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE* rocket = (sP_CL2FE_REQ_PC_ROCKET_STYLE_FIRE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // We should be sending back rocket succ packet, but it doesn't work, and this one works
    INITSTRUCT(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC, resp);
    resp.iToX = rocket->iToX;
    resp.iToY = rocket->iToY;
    // rocket->iToZ is broken, this seems like a good height
    resp.iToZ = plr->z + 100;

    resp.iBulletID = addBullet(plr, false);
    // we have to send it weapon id
    resp.Bullet.iID = plr->Equip[0].iID;
    resp.iBatteryW = plr->batteryW;

    sock->sendPacket(&resp, P_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC, sizeof(sP_FE2CL_REP_PC_GRENADE_STYLE_FIRE_SUCC));

    // send packet to nearby players
    INITSTRUCT(sP_FE2CL_PC_GRENADE_STYLE_FIRE, toOthers);
    toOthers.iPC_ID = plr->iID;
    toOthers.iToX = resp.iToX;
    toOthers.iToY = resp.iToY;
    toOthers.iToZ = resp.iToZ;
    toOthers.iBulletID = resp.iBulletID;
    toOthers.Bullet.iID = resp.Bullet.iID;

    PlayerManager::sendToViewable(sock, &toOthers, P_FE2CL_PC_GRENADE_STYLE_FIRE, sizeof(sP_FE2CL_PC_GRENADE_STYLE_FIRE));
}

static void projectileHit(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT* pkt = (sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    if (pkt->iTargetCnt == 0) {
        Bullets[plr->iID].erase(pkt->iBulletID);
        // no targets hit, don't send response
        return;
    }

    // sanity check
    if (!validInVarPacket(sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT), pkt->iTargetCnt, sizeof(int64_t), data->size)) {
        std::cout << "[WARN] bad sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT packet size\n";
        return;
    }

    // client sends us 8 bytes, where last 4 bytes are mob ID,
    // we use int64 pointer to move around but have to remember to cast it to int32
    int64_t* pktdata = (int64_t*)((uint8_t*)data->buf + sizeof(sP_CL2FE_REQ_PC_ROCKET_STYLE_HIT));

    /*
     * Due to the possibility of multiplication overflow (and regular buffer overflow),
     * both incoming and outgoing variable-length packets must be validated, at least if
     * the number of trailing structs isn't well known (ie. it's from the client).
     */
    if (!validOutVarPacket(sizeof(sP_FE2CL_PC_GRENADE_STYLE_HIT), pkt->iTargetCnt, sizeof(sAttackResult))) {
        std::cout << "[WARN] bad sP_FE2CL_PC_GRENADE_STYLE_HIT packet size\n";
        return;
    }

    /*
     * initialize response struct
     * rocket style hit doesn't work properly, so we're always sending this one
     */

    size_t resplen = sizeof(sP_FE2CL_PC_GRENADE_STYLE_HIT) + pkt->iTargetCnt * sizeof(sAttackResult);
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];

    memset(respbuf, 0, resplen);

    sP_FE2CL_PC_GRENADE_STYLE_HIT* resp = (sP_FE2CL_PC_GRENADE_STYLE_HIT*)respbuf;
    sAttackResult* respdata = (sAttackResult*)(respbuf + sizeof(sP_FE2CL_PC_GRENADE_STYLE_HIT));

    resp->iTargetCnt = pkt->iTargetCnt;
    if (Bullets.find(plr->iID) == Bullets.end() || Bullets[plr->iID].find(pkt->iBulletID) == Bullets[plr->iID].end()) {
        std::cout << "[WARN] projectileHit: bullet not found" << std::endl;
        return;
    }
    Bullet* bullet = &Bullets[plr->iID][pkt->iBulletID];

    for (int i = 0; i < pkt->iTargetCnt; i++) {
        if (NPCManager::NPCs.find(pktdata[i]) == NPCManager::NPCs.end()) {
            // not sure how to best handle this
            std::cout << "[WARN] projectileHit: NPC ID not found" << std::endl;
            return;
        }

        BaseNPC* npc = NPCManager::NPCs[pktdata[i]];
        if (npc->kind != EntityKind::MOB) {
            std::cout << "[WARN] projectileHit: NPC is not a mob" << std::endl;
            return;
        }

        Mob* mob = (Mob*)npc;
        std::pair<int, int> damage;

        damage.first = pkt->iTargetCnt > 1 ? bullet->groupDamage : bullet->pointDamage;

        int difficulty = (int)mob->data["m_iNpcLevel"];
        damage = getDamage(damage.first, (int)mob->data["m_iProtection"], true, bullet->weaponBoost, Nanos::nanoStyle(plr->activeNano), (int)mob->data["m_iNpcStyle"], difficulty);

        damage.first = mob->takeDamage(sock, damage.first);

        respdata[i].iID = mob->id;
        respdata[i].iDamage = damage.first;
        respdata[i].iHP = mob->hp;
        respdata[i].iHitFlag = damage.second;
    }

    resp->iPC_ID = plr->iID;
    resp->iBulletID = pkt->iBulletID;
    resp->Bullet.iID = bullet->bulletType;
    sock->sendPacket((void*)respbuf, P_FE2CL_PC_GRENADE_STYLE_HIT, resplen);
    PlayerManager::sendToViewable(sock, (void*)respbuf, P_FE2CL_PC_GRENADE_STYLE_HIT, resplen);

    Bullets[plr->iID].erase(resp->iBulletID);
}

static void playerTick(CNServer *serv, time_t currTime) {
    static time_t lastHealTime = 0;

    for (auto& pair : PlayerManager::players) {
        CNSocket *sock = pair.first;
        Player *plr = pair.second;
        bool transmit = false;

        // group ticks
        if (plr->group != nullptr)
            Groups::groupTickInfo(plr);

        // do not tick dead players
        if (plr->HP <= 0)
            continue;

        // fm patch/lake damage
        if ((plr->iConditionBitFlag & CSB_BIT_INFECTION)
            && !(plr->iSpecialState & CN_SPECIAL_STATE_FLAG__INVULNERABLE))
            dealGooDamage(sock, PC_MAXHEALTH(plr->level) * 3 / 20);

        // heal
        if (currTime - lastHealTime >= 4000 && !plr->inCombat && plr->HP < PC_MAXHEALTH(plr->level)) {
            if (currTime - lastHealTime - plr->healCooldown >= 4000) {
                plr->HP += PC_MAXHEALTH(plr->level) / 5;
                if (plr->HP > PC_MAXHEALTH(plr->level))
                    plr->HP = PC_MAXHEALTH(plr->level);
                transmit = true;
            } else
                plr->healCooldown -= 4000;
        }

        // nanos
        for (int i = 0; i < 3; i++) {
            if (plr->activeNano != 0 && plr->equippedNanos[i] == plr->activeNano) { // tick active nano
                sNano& nano = plr->Nanos[plr->activeNano];
                int drainRate = 0;

                if (Abilities::SkillTable.find(nano.iSkillID) != Abilities::SkillTable.end()) {
                    // nano has skill data
                    SkillData* skill = &Abilities::SkillTable[nano.iSkillID];
                    int boost = Nanos::getNanoBoost(plr);
                    drainRate = skill->batteryUse[boost * 3];

                    if (skill->drainType == SkillDrainType::PASSIVE) {
                        // passive buff
                        std::vector<EntityRef> targets;
                        if (skill->targetType == SkillTargetType::GROUP && plr->group != nullptr)
                            targets = plr->group->members; // group
                        else if(skill->targetType == SkillTargetType::SELF)
                            targets.push_back(sock); // self

                        std::cout << "[SKILL] id " << nano.iSkillID << ", type " << skill->skillType << ", target " << (int)skill->targetType << std::endl;
                    }
                }

                nano.iStamina -= 1 + drainRate / 5;

                if (nano.iStamina <= 0)
                    Nanos::summonNano(sock, -1, true); // unsummon nano silently

                transmit = true;
            } else if (plr->Nanos[plr->equippedNanos[i]].iStamina < 150) { // tick resting nano
                sNano& nano = plr->Nanos[plr->equippedNanos[i]];
                nano.iStamina += 1;

                if (nano.iStamina > 150)
                    nano.iStamina = 150;

                transmit = true;
            }
        }

        // check if the player has fallen out of the world
        if (plr->z < -30000) {
            INITSTRUCT(sP_FE2CL_PC_SUDDEN_DEAD, dead);

            dead.iPC_ID = plr->iID;
            dead.iDamage = plr->HP;
            dead.iHP = plr->HP = 0;

            sock->sendPacket((void*)&dead, P_FE2CL_PC_SUDDEN_DEAD, sizeof(sP_FE2CL_PC_SUDDEN_DEAD));
            PlayerManager::sendToViewable(sock, (void*)&dead, P_FE2CL_PC_SUDDEN_DEAD, sizeof(sP_FE2CL_PC_SUDDEN_DEAD));
        }

        if (transmit) {
            INITSTRUCT(sP_FE2CL_REP_PC_TICK, pkt);

            pkt.iHP = plr->HP;
            pkt.iBatteryN = plr->batteryN;

            pkt.aNano[0] = plr->Nanos[plr->equippedNanos[0]];
            pkt.aNano[1] = plr->Nanos[plr->equippedNanos[1]];
            pkt.aNano[2] = plr->Nanos[plr->equippedNanos[2]];

            sock->sendPacket((void*)&pkt, P_FE2CL_REP_PC_TICK, sizeof(sP_FE2CL_REP_PC_TICK));
        }
    }

    // if this was a heal tick, update the counter outside of the loop
    if (currTime - lastHealTime >= 4000)
        lastHealTime = currTime;
}

void Combat::init() {
    REGISTER_SHARD_TIMER(playerTick, 2000);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_NPCs, pcAttackNpcs);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_BEGIN, combatBegin);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_COMBAT_END, combatEnd);
    REGISTER_SHARD_PACKET(P_CL2FE_DOT_DAMAGE_ONOFF, dotDamageOnOff);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ATTACK_CHARs, pcAttackChars);

    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_GRENADE_STYLE_FIRE, grenadeFire);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ROCKET_STYLE_FIRE, rocketFire);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_ROCKET_STYLE_HIT, projectileHit);
}
