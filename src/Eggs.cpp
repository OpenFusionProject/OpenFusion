#include "Eggs.hpp"

#include "servers/CNShardServer.hpp"

#include "Player.hpp"
#include "PlayerManager.hpp"
#include "Abilities.hpp"
#include "NPCManager.hpp"
#include "Entities.hpp"
#include "Items.hpp"

#include <assert.h>

using namespace Eggs;

std::unordered_map<int, EggType> Eggs::EggTypes;

void Eggs::eggBuffPlayer(CNSocket* sock, int skillId, int eggId, int duration) {
    Player* plr = PlayerManager::getPlayer(sock);

    // eggId might be 0 if the buff is made by the /buff command
    EntityRef src = eggId == 0 ? sock : EntityRef(eggId);
    
    if(Abilities::SkillTable.count(skillId) == 0) {
        std::cout << "[WARN] egg " << eggId << " has skill ID " << skillId << " which doesn't exist" << std::endl;
        return;
    }

    SkillResult result = SkillResult();
    SkillData* skill = &Abilities::SkillTable[skillId];
    if(skill->drainType == SkillDrainType::PASSIVE) {
        // apply buff
        if(skill->targetType != SkillTargetType::PLAYERS) {
            std::cout << "[WARN] weird skill type for egg " << eggId << " with skill " << skillId << ", should be " << (int)skill->targetType << std::endl;
        }

        int timeBuffId = Abilities::getCSTBFromST(skill->skillType);
        int value = skill->values[0][0];
        BuffStack eggBuff = {
            duration * 1000 / MS_PER_PLAYER_TICK,
            value,
            src,
            BuffClass::EGG
        };
        plr->addBuff(timeBuffId,
            [](EntityRef self, Buff* buff, int status, BuffStack* stack) {
                Buffs::timeBuffUpdate(self, buff, status, stack);
                if(status == ETBU_DEL) Buffs::timeBuffTimeout(self);
            },
            [](EntityRef self, Buff* buff, time_t currTime) {
                // no-op
            },
            &eggBuff);

        sSkillResult_Buff resultBuff{};
        resultBuff.eCT = plr->getCharType();
        resultBuff.iID = plr->getID();
        resultBuff.bProtected = false;
        resultBuff.iConditionBitFlag = plr->getCompositeCondition();
        result = SkillResult(sizeof(sSkillResult_Buff), &resultBuff);
    } else {
        int value = plr->getMaxHP() * skill->values[0][0] / 1000;
        sSkillResult_Damage resultDamage{};
        sSkillResult_Heal_HP resultHeal{};
        switch(skill->skillType)
        {
            case SkillType::DAMAGE:
                resultDamage.bProtected = false;
                resultDamage.eCT = plr->getCharType();
                resultDamage.iID = plr->getID();
                resultDamage.iDamage = plr->takeDamage(src, value);
                resultDamage.iHP = plr->getCurrentHP();
                result = SkillResult(sizeof(sSkillResult_Damage), &resultDamage);
                break;
            case SkillType::HEAL_HP:
                resultHeal.eCT = plr->getCharType();
                resultHeal.iID = plr->getID();
                resultHeal.iHealHP = plr->heal(src, value);
                resultHeal.iHP = plr->getCurrentHP();
                result = SkillResult(sizeof(sSkillResult_Heal_HP), &resultHeal);
                break;
            default:
                std::cout << "[WARN] oops, egg with active skill type " << (int)skill->skillType << " unhandled";
                return;
        }
    }

    // initialize response struct
    size_t resplen = sizeof(sP_FE2CL_NPC_SKILL_HIT) + result.size;
    uint8_t respbuf[CN_PACKET_BUFFER_SIZE];
    memset(respbuf, 0, resplen);

    sP_FE2CL_NPC_SKILL_HIT* pkt = (sP_FE2CL_NPC_SKILL_HIT*)respbuf;
    pkt->iNPC_ID = eggId;
    pkt->iSkillID = skillId;
    pkt->eST = (int32_t)skill->skillType;
    pkt->iTargetCnt = 1;

    if(result.size > 0) {
        void* attached = (void*)(pkt + 1);
        memcpy(attached, result.payload, result.size);
    }
    
    NPCManager::sendToViewable(src.getEntity(), pkt, P_FE2CL_NPC_SKILL_HIT, resplen);
}

static void eggStep(CNServer* serv, time_t currTime) {
    // check dead eggs and eggs in inactive chunks
    for (auto npc : NPCManager::NPCs) {
        if (npc.second->kind != EntityKind::EGG)
            continue;

        auto egg = (Egg*)npc.second;
        if (!egg->dead || !Chunking::inPopulatedChunks(&egg->viewableChunks))
            continue;

        if (egg->deadUntil <= currTime) {
            // respawn it
            egg->dead = false;
            egg->deadUntil = 0;
            egg->hp = 400;
            
            Chunking::addEntityToChunks(Chunking::getViewableChunks(egg->chunkPos), {npc.first});
        }
    }

}

void Eggs::npcDataToEggData(int x, int y, int z, sNPCAppearanceData* npc, sShinyAppearanceData* egg) {
    egg->iX = x;
    egg->iY = y;
    egg->iZ = z;
    // client doesn't care about egg->iMapNum
    egg->iShinyType = npc->iNPCType;
    egg->iShiny_ID = npc->iNPC_ID;
}

static void eggPickup(CNSocket* sock, CNPacketData* data) {
    auto pickup = (sP_CL2FE_REQ_SHINY_PICKUP*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    EntityRef eggRef = {pickup->iShinyID};

    if (!eggRef.isValid()) {
        std::cout << "[WARN] Player tried to open non existing egg?!" << std::endl;
        return;
    }
    auto egg = (Egg*)eggRef.getEntity();
    if (egg->kind != EntityKind::EGG) {
        std::cout << "[WARN] Player tried to open something other than an?!" << std::endl;
        return;
    }

    if (egg->dead) {
        std::cout << "[WARN] Player tried to open a dead egg?!" << std::endl;
        return;
    }

    /* this has some issues with position desync, leaving it out for now
    if (abs(egg->x - plr->x)>500 || abs(egg->y - plr->y) > 500) {
        std::cout << "[WARN] Player tried to open an egg isn't nearby?!" << std::endl;
        return;
    }
    */

    int typeId = egg->type;
    if (EggTypes.find(typeId) == EggTypes.end()) {
        std::cout << "[WARN] Egg Type " << typeId << " not found!" << std::endl;
        return;
    }

    EggType* type = &EggTypes[typeId];

    /*
     * SHINY_PICKUP_SUCC is only causing a GUI effect in the client
     * (buff icon pops up in the bottom of the screen)
     * so we don't send it for non-effect
     */
    if (type->effectId != 0) {
        eggBuffPlayer(sock, type->effectId, eggRef.id, type->duration);
        INITSTRUCT(sP_FE2CL_REP_SHINY_PICKUP_SUCC, resp);
        resp.iSkillID = type->effectId;

        // in general client finds correct icon on it's own,
        // but for damage we have to supply correct CSTB
        if (resp.iSkillID == 183)
            resp.eCSTB = ECSB_INFECTION;

        sock->sendPacket(resp, P_FE2CL_REP_SHINY_PICKUP_SUCC);
    }

    // drop
    if (type->dropCrateId != 0) {
        const size_t resplen = sizeof(sP_FE2CL_REP_REWARD_ITEM) + sizeof(sItemReward);
        assert(resplen < CN_PACKET_BUFFER_SIZE - 8);
        // we know it's only one trailing struct, so we can skip full validation

        uint8_t respbuf[resplen]; // not a variable length array, don't worry
        sP_FE2CL_REP_REWARD_ITEM* reward = (sP_FE2CL_REP_REWARD_ITEM*)respbuf;
        sItemReward* item = (sItemReward*)(respbuf + sizeof(sP_FE2CL_REP_REWARD_ITEM));

        // don't forget to zero the buffer!
        memset(respbuf, 0, resplen);

        // send back player's stats
        reward->m_iCandy = plr->money;
        reward->m_iFusionMatter = plr->fusionmatter;
        reward->m_iBatteryN = plr->batteryN;
        reward->m_iBatteryW = plr->batteryW;
        reward->iFatigue = 100; // prevents warning message
        reward->iFatigue_Level = 1;
        reward->iItemCnt = 1; // remember to update resplen if you change this

        int slot = Items::findFreeSlot(plr);

        // no space for drop
        if (slot != -1) {

            // item reward
            item->sItem.iType = 9;
            item->sItem.iOpt = 1;
            item->sItem.iID = type->dropCrateId;
            item->iSlotNum = slot;
            item->eIL = 1; // Inventory Location. 1 means player inventory.

            // update player
            plr->Inven[slot] = item->sItem;
            sock->sendPacket((void*)respbuf, P_FE2CL_REP_REWARD_ITEM, resplen);
        }
    }

    if (egg->summoned)
        NPCManager::destroyNPC(eggRef.id);
    else {
        Chunking::removeEntityFromChunks(Chunking::getViewableChunks(egg->chunkPos), eggRef);
        egg->dead = true;
        egg->deadUntil = getTime() + (time_t)type->regen * 1000;
        egg->hp = 0;
    }
}

void Eggs::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SHINY_PICKUP, eggPickup);

    REGISTER_SHARD_TIMER(eggStep, 1000);
}
