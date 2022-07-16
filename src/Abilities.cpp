#include "Abilities.hpp"

#include "NPCManager.hpp"
#include "PlayerManager.hpp"

std::map<int32_t, SkillData> Abilities::SkillTable;

/*
// New email notification
static void emailUpdateCheck(CNSocket* sock, CNPacketData* data) {
    INITSTRUCT(sP_FE2CL_REP_PC_NEW_EMAIL, resp);
    resp.iNewEmailCnt = Database::getUnreadEmailCount(PlayerManager::getPlayer(sock)->iID);
    sock->sendPacket(resp, P_FE2CL_REP_PC_NEW_EMAIL);
}
*/

std::vector<EntityRef> Abilities::matchTargets(SkillData* skill, int count, int32_t *ids) {

    std::vector<EntityRef> targets;

    for (int i = 0; i < count; i++) {
        int32_t id = ids[i];
        if (skill->targetType == SkillTargetType::MOBS) {
            // mob?
            if (NPCManager::NPCs.find(id) != NPCManager::NPCs.end()) targets.push_back(id);
            else std::cout << "[WARN] skill: id not found\n";
        } else {
            // player?
            CNSocket* sock = PlayerManager::getSockFromID(id);
            if (sock != nullptr) targets.push_back(sock);
            else std::cout << "[WARN] skill: sock not found\n";
        }
    }

    return targets; 
}

void Abilities::applyAbility(SkillData* skill, EntityRef src, std::vector<EntityRef> targets) {
    for (EntityRef target : targets) {
        Entity* entity = target.getEntity();
        if (entity->kind != PLAYER && entity->kind != COMBAT_NPC && entity->kind != MOB)
            continue; // not a combatant


    }
}

void Abilities::init() {
    //REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_EMAIL_UPDATE_CHECK, emailUpdateCheck);
}
