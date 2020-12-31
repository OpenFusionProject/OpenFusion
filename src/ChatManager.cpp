#include "CNShardServer.hpp"
#include "CNStructs.hpp"
#include "ChatManager.hpp"
#include "PlayerManager.hpp"
#include "TransportManager.hpp"
#include "TableData.hpp"
#include "NPCManager.hpp"
#include "MobManager.hpp"
#include "MissionManager.hpp"
#include "ChunkManager.hpp"
#include "ItemManager.hpp"

#include <sstream>
#include <iterator>
#include <math.h>

std::map<std::string, ChatCommand> ChatManager::commands;
std::vector<std::string> ChatManager::dump;

std::vector<std::string> parseArgs(std::string full) {
    std::stringstream ss(full);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    return std::vector<std::string>(begin, end);
}

bool ChatManager::runCmd(std::string full, CNSocket* sock) {
    std::vector<std::string> args = parseArgs(full);
    std::string cmd = args[0].substr(1, args[0].size() - 1);

    // check if the command exists
    if (ChatManager::commands.find(cmd) != ChatManager::commands.end()) {
        Player* plr = PlayerManager::getPlayer(sock);
        ChatCommand command = ChatManager::commands[cmd];

        // sanity check + does the player have the required account level to use the command?
        if (plr != nullptr && plr->accountLevel <= command.requiredAccLevel) {
            command.handlr(full, args, sock);
            return true;
        } else {
            ChatManager::sendServerMessage(sock, "You don't have access to that command!");
            return false;
        }
    }

    ChatManager::sendServerMessage(sock, "Unknown command!");
    return false;
}

void helpCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "Commands available to you:");
    Player *plr = PlayerManager::getPlayer(sock);

    for (auto& cmd : ChatManager::commands) {
        if (cmd.second.requiredAccLevel >= plr->accountLevel)
            ChatManager::sendServerMessage(sock, "/" + cmd.first + (cmd.second.help.length() > 0 ? " - " + cmd.second.help : ""));
    }
}

void accessCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "Your access level is " + std::to_string(PlayerManager::getPlayer(sock)->accountLevel));
}

void populationCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, std::to_string(PlayerManager::players.size()) + " players online");
}

void levelCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "/level: no level specified");
        return;
    }

    Player *plr = PlayerManager::getPlayer(sock);

    char *tmp;
    int level = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if ((level < 1 || level > 36) && plr->accountLevel > 30)
        return;

    if (!(level < 1 || level > 36))
        plr->level = level;

    INITSTRUCT(sP_FE2CL_REP_PC_CHANGE_LEVEL, resp);

    resp.iPC_ID = plr->iID;
    resp.iPC_Level = level;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_PC_CHANGE_LEVEL, sizeof(sP_FE2CL_REP_PC_CHANGE_LEVEL));
}

void mssCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "[MSS] Too few arguments");
        ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test/export> <<height>>");
        return;
    }

    // Validate route number
    char* routeNumC;
    int routeNum = std::strtol(args[1].c_str(), &routeNumC, 10);
    if (*routeNumC) {
        // not an integer
        ChatManager::sendServerMessage(sock, "[MSS] Invalid route number '" + args[1] + "'");
        return;
    }

    if (args.size() < 3) {
        ChatManager::sendServerMessage(sock, "[MSS] Too few arguments");
        ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test> <<height>>");
        return;
    }

    // get the route (if it doesn't exist yet, this will also make it)
    std::vector<WarpLocation>* route = &TableData::RunningSkywayRoutes[routeNum];

    // mss <route> add <height>
    if (args[2] == "add") {
        // make sure height token exists
        if (args.size() < 4) {
            ChatManager::sendServerMessage(sock, "[MSS] Point height must be specified");
            ChatManager::sendServerMessage(sock, "[MSS] Usage: /mss <route> add <height>");
            return;
        }
        // validate height token
        char* heightC;
        int height = std::strtol(args[3].c_str(), &heightC, 10);
        if (*heightC) {
            ChatManager::sendServerMessage(sock, "[MSS] Invalid height " + args[3]);
            return;
        }

        Player* plr = PlayerManager::getPlayer(sock);
        route->push_back({ plr->x, plr->y, height }); // add point
        ChatManager::sendServerMessage(sock, "[MSS] Added point (" + std::to_string(plr->x) + ", " + std::to_string(plr->y) + ", " + std::to_string(height) + ") to route " + std::to_string(routeNum));
        return;
    }

    // mss <route> remove
    if (args[2] == "remove") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->back();
        route->pop_back(); // remove point at top of stack
        ChatManager::sendServerMessage(sock, "[MSS] Removed point (" + std::to_string(pulled.x) + ", " + std::to_string(pulled.y) + ", " + std::to_string(pulled.z) + ") from route " + std::to_string(routeNum));
        return;
    }

    // mss <route> goto
    if (args[2] == "goto") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->back();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        return;
    }

    // mss <route> clear
    if (args[2] == "clear") {
        route->clear();
        ChatManager::sendServerMessage(sock, "[MSS] Cleared route " + std::to_string(routeNum));
        return;
    }

    // mss <route> test
    if (args[2] == "test") {
        if (route->empty()) {
            ChatManager::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        WarpLocation pulled = route->front();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        TransportManager::testMssRoute(sock, route);
        return;
    }

    // for compatibility: mss <route> export
    if (args[2] == "export") {
        ChatManager::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
        TableData::flush();
        return;
    }

    // mss ????
    ChatManager::sendServerMessage(sock, "[MSS] Unknown command '" + args[2] + "'");
}

void summonWCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "/summonW: no mob type specified");
        return;
    }
    Player* plr = PlayerManager::getPlayer(sock);

    char *rest;
    int type = std::strtol(args[1].c_str(), &rest, 10);
    if (*rest) {
        ChatManager::sendServerMessage(sock, "Invalid NPC number: " + args[1]);
        return;
    }

    int limit = NPCManager::NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (type > limit)
        return;

    BaseNPC *npc = NPCManager::summonNPC(plr->x, plr->y, plr->z, plr->instanceID, type, true);

    // update angle
    npc->appearanceData.iAngle = (plr->angle + 180) % 360;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, plr->instanceID, npc->appearanceData.iAngle);

    // if we're in a lair, we need to spawn the NPC in both the private instance and the template
    if (PLAYERID(plr->instanceID) != 0) {
        npc = NPCManager::summonNPC(plr->x, plr->y, plr->z, plr->instanceID, type, true, true);

        npc->appearanceData.iAngle = (plr->angle + 180) % 360;
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, npc->instanceID, npc->appearanceData.iAngle);
    }

    ChatManager::sendServerMessage(sock, "/summonW: placed mob with type: " + std::to_string(type) +
        ", id: " + std::to_string(npc->appearanceData.iNPC_ID));
    TableData::RunningMobs[npc->appearanceData.iNPC_ID] = npc; // only record the one in the template
}

void unsummonWCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC* npc = NPCManager::getNearestNPC(plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        ChatManager::sendServerMessage(sock, "/unsummonW: No NPCs found nearby");
        return;
    }

    if (TableData::RunningEggs.find(npc->appearanceData.iNPC_ID) != TableData::RunningEggs.end()) {
        ChatManager::sendServerMessage(sock, "/unsummonW: removed egg with type: " + std::to_string(npc->appearanceData.iNPCType) +
            ", id: " + std::to_string(npc->appearanceData.iNPC_ID));
        TableData::RunningEggs.erase(npc->appearanceData.iNPC_ID);
        NPCManager::destroyNPC(npc->appearanceData.iNPC_ID);
        return;
    }

    if (TableData::RunningMobs.find(npc->appearanceData.iNPC_ID) == TableData::RunningMobs.end()
        && TableData::RunningGroups.find(npc->appearanceData.iNPC_ID) == TableData::RunningGroups.end()) {
        ChatManager::sendServerMessage(sock, "/unsummonW: Closest NPC is not a gruntwork mob.");
        return;
    }

    if (MobManager::Mobs.find(npc->appearanceData.iNPC_ID) != MobManager::Mobs.end()) {
        int leadId = ((Mob*)npc)->groupLeader;
        if (leadId != 0) {
            if (MobManager::Mobs.find(leadId) == MobManager::Mobs.end()) {
                std::cout << "[WARN] unsummonW: leader not found!" << std::endl;
            }
            Mob* leadNpc = MobManager::Mobs[leadId];
            for (int i = 0; i < 4; i++) {
                if (leadNpc->groupMember[i] == 0)
                    break;

                if (MobManager::Mobs.find(leadNpc->groupMember[i]) == MobManager::Mobs.end()) {
                    std::cout << "[WARN] unsommonW: leader can't find a group member!" << std::endl;
                    continue;
                }

                NPCManager::destroyNPC(leadNpc->groupMember[i]);
            }
            TableData::RunningGroups.erase(leadId);
            NPCManager::destroyNPC(leadId);
            ChatManager::sendServerMessage(sock, "/unsummonW: Mob group destroyed.");
            return;
        }
    }

    ChatManager::sendServerMessage(sock, "/unsummonW: removed mob with type: " + std::to_string(npc->appearanceData.iNPCType) +
        ", id: " + std::to_string(npc->appearanceData.iNPC_ID));

    TableData::RunningMobs.erase(npc->appearanceData.iNPC_ID);

    NPCManager::destroyNPC(npc->appearanceData.iNPC_ID);
}

void toggleAiCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    MobManager::simulateMobs = !MobManager::simulateMobs;

    if (MobManager::simulateMobs)
        return;

    // return all mobs to their spawn points
    for (auto& pair : MobManager::Mobs) {
        pair.second->state = MobState::RETREAT;
        pair.second->target = nullptr;
        pair.second->nextMovement = getTime();

        // mobs with static paths can chill where they are
        if (pair.second->staticPath) {
            pair.second->roamX = pair.second->appearanceData.iX;
            pair.second->roamY = pair.second->appearanceData.iY;
            pair.second->roamZ = pair.second->appearanceData.iZ;
        } else {
            pair.second->roamX = pair.second->spawnX;
            pair.second->roamY = pair.second->spawnY;
            pair.second->roamZ = pair.second->spawnZ;
        }
    }
}

void npcRotateCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC* npc = NPCManager::getNearestNPC(plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        ChatManager::sendServerMessage(sock, "[NPCR] No NPCs found nearby");
        return;
    }

    int angle = (plr->angle + 180) % 360;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ, npc->instanceID, angle);

    // if it's a gruntwork NPC, rotate in-place
    if (TableData::RunningMobs.find(npc->appearanceData.iNPC_ID) != TableData::RunningMobs.end()) {
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ, npc->instanceID, angle);

        ChatManager::sendServerMessage(sock, "[NPCR] Successfully set angle to " + std::to_string(angle) + " for gruntwork NPC "
            + std::to_string(npc->appearanceData.iNPC_ID));
    } else {
        TableData::RunningNPCRotations[npc->appearanceData.iNPC_ID] = angle;

        ChatManager::sendServerMessage(sock, "[NPCR] Successfully set angle to " + std::to_string(angle) + " for NPC "
            + std::to_string(npc->appearanceData.iNPC_ID));
    }

    // update rotation clientside
    INITSTRUCT(sP_FE2CL_NPC_ENTER, pkt);
    pkt.NPCAppearanceData = npc->appearanceData;
    sock->sendPacket((void*)&pkt, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
}

void refreshCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    ChunkPos currentChunk = plr->chunkPos;
    ChunkPos nullChunk = std::make_tuple(0, 0, 0);
    ChunkManager::updatePlayerChunk(sock, currentChunk, nullChunk);
    ChunkManager::updatePlayerChunk(sock, nullChunk, currentChunk);
}

void instanceCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {

    Player* plr = PlayerManager::getPlayer(sock);

    // no additional arguments: report current instance ID
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "[INST] Current instance ID: " + std::to_string(plr->instanceID));
        ChatManager::sendServerMessage(sock, "[INST] (Map " + std::to_string(MAPNUM(plr->instanceID)) + ", instance " + std::to_string(PLAYERID(plr->instanceID)) + ")");
        return;
    }

    // move player to specified instance
    // validate instance ID
    char* instanceS;
    uint64_t instance = std::strtoll(args[1].c_str(), &instanceS, 10);
    if (*instanceS) {
        ChatManager::sendServerMessage(sock, "[INST] Invalid instance ID: " + args[1]);
        return;
    }

    if (args.size() >= 3) {
        char* playeridS;
        uint64_t playerid = std::strtoll(args[2].c_str(), &playeridS, 10);

        if (playerid != 0) {
            instance |= playerid << 32ULL;
            ChunkManager::createInstance(instance);

            // a precaution
            plr->recallInstance = 0;
        }
    }

    PlayerManager::sendPlayerTo(sock, plr->x, plr->y, plr->z, instance);
    ChatManager::sendServerMessage(sock, "[INST] Switched to instance with ID " + std::to_string(instance));
}

void npcInstanceCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "[NPCI] Instance ID must be specified");
        ChatManager::sendServerMessage(sock, "[NPCI] Usage: /npci <instance ID>");
        return;
    }

    BaseNPC* npc = NPCManager::getNearestNPC(plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        ChatManager::sendServerMessage(sock, "[NPCI] No NPCs found nearby");
        return;
    }

    // validate instance ID
    char* instanceS;
    int instance = std::strtol(args[1].c_str(), &instanceS, 10);
    if (*instanceS) {
        ChatManager::sendServerMessage(sock, "[NPCI] Invalid instance ID: " + args[1]);
        return;
    }

    ChatManager::sendServerMessage(sock, "[NPCI] Moving NPC with ID " + std::to_string(npc->appearanceData.iNPC_ID) + " to instance " + std::to_string(instance));
    TableData::RunningNPCMapNumbers[npc->appearanceData.iNPC_ID] = instance;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ, instance, npc->appearanceData.iAngle);
}

void minfoCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    ChatManager::sendServerMessage(sock, "[MINFO] Current mission ID: " + std::to_string(plr->CurrentMissionID));

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] != 0) {
            TaskData& task = *MissionManager::Tasks[plr->tasks[i]];
            if ((int)(task["m_iHMissionID"]) == plr->CurrentMissionID) {
                ChatManager::sendServerMessage(sock, "[MINFO] Current task ID: " + std::to_string(plr->tasks[i]));
                ChatManager::sendServerMessage(sock, "[MINFO] Current task type: " + std::to_string((int)(task["m_iHTaskType"])));
                ChatManager::sendServerMessage(sock, "[MINFO] Current waypoint NPC ID: " + std::to_string((int)(task["m_iSTGrantWayPoint"])));
                ChatManager::sendServerMessage(sock, "[MINFO] Current terminator NPC ID: " + std::to_string((int)(task["m_iHTerminatorNPCID"])));

                if ((int)(task["m_iSTGrantTimer"]) != 0)
                    ChatManager::sendServerMessage(sock, "[MINFO] Current task timer: " + std::to_string((int)(task["m_iSTGrantTimer"])));

                for (int j = 0; j < 3; j++)
                    if ((int)(task["m_iCSUEnemyID"][j]) != 0)
                        ChatManager::sendServerMessage(sock, "[MINFO] Current task mob #" + std::to_string(j+1) +": " + std::to_string((int)(task["m_iCSUEnemyID"][j])));

                return;
            }
        }
    }
}

void tasksCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {

    Player* plr = PlayerManager::getPlayer(sock);

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] != 0) {
            TaskData& task = *MissionManager::Tasks[plr->tasks[i]];
            ChatManager::sendServerMessage(sock, "[TASK-" + std::to_string(i) + "] mission ID: " + std::to_string((int)(task["m_iHMissionID"])));
            ChatManager::sendServerMessage(sock, "[TASK-" + std::to_string(i) + "] task ID: " + std::to_string(plr->tasks[i]));
        }
    }
}

void buffCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 3) {
        ChatManager::sendServerMessage(sock, "/buff: no skill Id and duration time specified");
        return;
    }

    char* tmp;
    int  skillId = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;
    int  duration = std::strtol(args[2].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if (NPCManager::eggBuffPlayer(sock, skillId, duration)<0)
        ChatManager::sendServerMessage(sock, "/buff: unknown skill Id");
    
}

void eggCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "/egg: no egg type specified");
        return;
    }

    char* tmp;
    int  eggType = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if (NPCManager::EggTypes.find(eggType) == NPCManager::EggTypes.end()) {
        ChatManager::sendServerMessage(sock, "/egg: Unknown egg type");
        return;
    }

    assert(NPCManager::nextId < INT32_MAX);
    int id = NPCManager::nextId++;

    Player* plr = PlayerManager::getPlayer(sock);

    // some math to place egg nicely in front of the player
    // temporarly disabled for sake of gruntwork
    int addX = 0; //-500.0f * sin(plr->angle / 180.0f * M_PI);
    int addY = 0;  //-500.0f * cos(plr->angle / 180.0f * M_PI);

    Egg* egg = new Egg(plr->x + addX, plr->y + addY, plr->z, plr->instanceID, eggType, id, false); // change last arg to true after gruntwork
    NPCManager::NPCs[id] = egg;
    NPCManager::Eggs[id] = egg;
    NPCManager::updateNPCPosition(id, plr->x + addX, plr->y + addY, plr->z, plr->instanceID, plr->angle);

    // add to template
    TableData::RunningEggs[id] = egg;
}

void notifyCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr->notify) {
        plr->notify = false;
        ChatManager::sendServerMessage(sock, "[ADMIN] No longer receiving join notifications");
    } else {
        plr->notify = true;
        ChatManager::sendServerMessage(sock, "[ADMIN] Receiving join notifications");
    }
}

void playersCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    ChatManager::sendServerMessage(sock, "[ADMIN] Players on the server:");
    for (auto pair : PlayerManager::players)
        ChatManager::sendServerMessage(sock, PlayerManager::getPlayerName(pair.second));
}

void summonGroupCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 4) {
        ChatManager::sendServerMessage(sock, "/summonGroup(W) <leadermob> <mob> <number> [distance]");
        return;
    }
    Player* plr = PlayerManager::getPlayer(sock);

    char *rest;
    
    bool wCommand = (args[0] == "/summonGroupW");
    int type = std::strtol(args[1].c_str(), &rest, 10);
    int type2 = std::strtol(args[2].c_str(), &rest, 10);
    int count = std::strtol(args[3].c_str(), &rest, 10);
    int distance = 150;
    if (args.size() > 4)
        distance = std::strtol(args[4].c_str(), &rest, 10);

    if (*rest) {
        ChatManager::sendServerMessage(sock, "Invalid NPC number: " + args[1]);
        return;
    }

    int limit = NPCManager::NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (type > limit || type2 > limit || count > 5) {
        ChatManager::sendServerMessage(sock, "Invalid parameters; double check types and count");
        return;
    }

    Mob* leadNpc = nullptr;

    for (int i = 0; i < count; i++) {
        int team = NPCManager::NPCData[type]["m_iTeam"];
        int x = plr->x;
        int y = plr->y;
        int z = plr->z;

        if (i > 0) {
            int angle = 360.0f / (count-1) * (i-1);
            if (count == 3)
                angle = 90 + 60 * i;

            angle += (plr->angle + 180) % 360;

            x += -1.0f * sin(angle / 180.0f * M_PI) * distance;
            y += -1.0f * cos(angle / 180.0f * M_PI) * distance;
            z = plr->z;
        }

        BaseNPC *npc = NPCManager::summonNPC(x, y, z, plr->instanceID, type, wCommand);
        if (team == 2 && i > 0) {
            leadNpc->groupMember[i-1] = npc->appearanceData.iNPC_ID;
            Mob* mob = MobManager::Mobs[npc->appearanceData.iNPC_ID];
            mob->groupLeader = leadNpc->appearanceData.iNPC_ID;
            mob->offsetX = x - plr->x;
            mob->offsetY = y - plr->y;
        }

        npc->appearanceData.iAngle = (plr->angle + 180) % 360;
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, x, y, z, plr->instanceID, npc->appearanceData.iAngle);

        // if we're in a lair, we need to spawn the NPC in both the private instance and the template
        if (PLAYERID(plr->instanceID) != 0) {
            npc = NPCManager::summonNPC(plr->x, plr->y, plr->z, plr->instanceID, type, wCommand, true);

            if (team == 2 && i > 0) {
                leadNpc->groupMember[i-1] = npc->appearanceData.iNPC_ID;
                Mob* mob = MobManager::Mobs[npc->appearanceData.iNPC_ID];
                mob->groupLeader = leadNpc->appearanceData.iNPC_ID;
                mob->offsetX = x - plr->x;
                mob->offsetY = y - plr->y;
            }

            npc->appearanceData.iAngle = (plr->angle + 180) % 360;
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, x, y, z, plr->instanceID, npc->appearanceData.iAngle);
        }

        ChatManager::sendServerMessage(sock, "/summonGroup(W): placed mob with type: " + std::to_string(type) +
            ", id: " + std::to_string(npc->appearanceData.iNPC_ID));

        if (i == 0 && team == 2) {
            type = type2;
            leadNpc = MobManager::Mobs[npc->appearanceData.iNPC_ID];
            leadNpc->groupLeader = leadNpc->appearanceData.iNPC_ID;
        }
    }

    if (!wCommand)
        return; // not writing; don't add to running mobs

    if (leadNpc == nullptr) {
        std::cout << "/summonGroupW: can't find group leader! Won't be saved!\n";
        return;
    }

    TableData::RunningGroups[leadNpc->appearanceData.iNPC_ID] = leadNpc; // only record the leader
}

void flushCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    TableData::flush();
    ChatManager::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
}

void whoisCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    BaseNPC* npc = NPCManager::getNearestNPC(plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        ChatManager::sendServerMessage(sock, "[WHOIS] No NPCs found nearby");
        return;
    }

    ChatManager::sendServerMessage(sock, "[WHOIS] ID: " + std::to_string(npc->appearanceData.iNPC_ID));
    ChatManager::sendServerMessage(sock, "[WHOIS] Type: " + std::to_string(npc->appearanceData.iNPCType));
    ChatManager::sendServerMessage(sock, "[WHOIS] HP: " + std::to_string(npc->appearanceData.iHP));
    ChatManager::sendServerMessage(sock, "[WHOIS] CBF: " + std::to_string(npc->appearanceData.iConditionBitFlag));
    ChatManager::sendServerMessage(sock, "[WHOIS] Class: " + std::to_string(npc->npcClass));
    ChatManager::sendServerMessage(sock, "[WHOIS] X: " + std::to_string(npc->appearanceData.iX));
    ChatManager::sendServerMessage(sock, "[WHOIS] Y: " + std::to_string(npc->appearanceData.iY));
    ChatManager::sendServerMessage(sock, "[WHOIS] Z: " + std::to_string(npc->appearanceData.iZ));
    ChatManager::sendServerMessage(sock, "[WHOIS] Angle: " + std::to_string(npc->appearanceData.iAngle));
    std::string chunkPosition = std::to_string(std::get<0>(npc->chunkPos)) + ", " + std::to_string(std::get<1>(npc->chunkPos)) + ", " + std::to_string(std::get<2>(npc->chunkPos));
    ChatManager::sendServerMessage(sock, "[WHOIS] Chunk: {" + chunkPosition + "}");
    ChatManager::sendServerMessage(sock, "[WHOIS] MapNum: " + std::to_string(MAPNUM(npc->instanceID)));
    ChatManager::sendServerMessage(sock, "[WHOIS] Instance: " + std::to_string(PLAYERID(npc->instanceID)));
}

void lairUnlockCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (!ChunkManager::chunkExists(plr->chunkPos))
        return;

    Chunk* chnk = ChunkManager::chunks[plr->chunkPos];
    int taskID = -1;
    int missionID = -1;
    int found = 0;
    for (int32_t id : chnk->NPCs) {
        if (NPCManager::NPCs.find(id) == NPCManager::NPCs.end())
            continue;

        BaseNPC* npc = NPCManager::NPCs[id];
        for (auto it = NPCManager::Warps.begin(); it != NPCManager::Warps.end(); it++) {
            if ((*it).second.npcID == npc->appearanceData.iNPCType) {
                taskID = (*it).second.limitTaskID;
                missionID = MissionManager::Tasks[taskID]->task["m_iHMissionID"];
                found++;
                break;
            }
        }
    }

    if (missionID == -1 || taskID == -1) {
        ChatManager::sendServerMessage(sock, "You are NOT standing near a lair portal; move around and try again!");
        return;
    }

    if (found > 1) {
        ChatManager::sendServerMessage(sock, "More than one lair found; decrease chunk size and try again!");
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TASK_START_SUCC, taskResp);
    MissionManager::startTask(plr, taskID);
    taskResp.iTaskNum = taskID;
    taskResp.iRemainTime = 0;
    sock->sendPacket((void*)&taskResp, P_FE2CL_REP_PC_TASK_START_SUCC, sizeof(sP_FE2CL_REP_PC_TASK_START_SUCC));

    INITSTRUCT(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID, missionResp);
    missionResp.iCurrentMissionID = missionID;
    plr->CurrentMissionID = missionID;
    sock->sendPacket((void*)&missionResp, P_FE2CL_REP_PC_SET_CURRENT_MISSION_ID, sizeof(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID));
}

void hideCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (plr->hidden) {
        ChatManager::sendServerMessage(sock, "[HIDE] You're already hidden from the map.");
        return;
    }

    plr->hidden = true;
    ChatManager::sendServerMessage(sock, "[HIDE] Successfully hidden from the map.");
}

void unhideCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (!plr->hidden) {
        ChatManager::sendServerMessage(sock, "[HIDE] You're already visible from the map.");
        return;
    }

    plr->hidden = false;
    ChatManager::sendServerMessage(sock, "[HIDE] Successfully un-hidden from the map.");
}

void redeemCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        ChatManager::sendServerMessage(sock, "/redeem: No code specified");
        return;
    }

    // convert string to all lowercase
    const char* codeRaw = args[1].c_str();
    if (args[1].size() > 256) { // prevent overflow
        ChatManager::sendServerMessage(sock, "/redeem: Code too long");
        return;
    }

    char buf[256];
    for (int i = 0; i < args[1].size(); i++)
        buf[i] = std::tolower(codeRaw[i]);
    std::string code(buf, args[1].size());

    if (ItemManager::CodeItems.find(code) == ItemManager::CodeItems.end()) {
        ChatManager::sendServerMessage(sock, "/redeem: Unknown code");
        return;
    }

    Player* plr = PlayerManager::getPlayer(sock);
    int itemCount = ItemManager::CodeItems[code].size();
    int slots[4];

    for (int i = 0; i < itemCount; i++) {
        slots[i] = ItemManager::findFreeSlot(plr);
        if (slots[i] == -1) {
            ChatManager::sendServerMessage(sock, "/redeem: Not enough space in inventory");

            // delete any temp items we might have set
            for (int j = 0; j < i; j++) {
                plr->Inven[slots[j]] = { 0, 0, 0, 0 }; // empty
            }
            return;
        }

        plr->Inven[slots[i]] = { 999, 999, 999, 0 }; // temp item; overwritten later
    }
    
    for (int i = 0; i < itemCount; i++) {
        std::pair<int32_t, int32_t> item = ItemManager::CodeItems[code][i];
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = 1;
        resp.iSlotNum = slots[i];
        resp.Item.iID = item.first;
        resp.Item.iType = item.second;
        // I think it is safe? :eyes
        resp.Item.iOpt = 1;

        // save serverside
        plr->Inven[resp.iSlotNum] = resp.Item;

        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC, sizeof(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC));
    }
    std::string msg = itemCount == 1 ? "You have redeemed a code item" : "You have redeemed code items";
    ChatManager::sendServerMessage(sock, msg);
}

void unwarpableCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);
    plr->unwarpable = true;
}

void warpableCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);
    plr->unwarpable = false;
}

void ChatManager::init() {
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_FREECHAT_MESSAGE, chatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT, emoteHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_SEND_MENUCHAT_MESSAGE, menuChatHandler);
    REGISTER_SHARD_PACKET(P_CL2FE_GM_REQ_PC_ANNOUNCE, announcementHandler);

    registerCommand("help", 100, helpCommand, "list all unlocked server-side commands");
    registerCommand("access", 100, accessCommand, "print your access level");
    registerCommand("instance", 30, instanceCommand, "print or change your current instance");
    registerCommand("mss", 30, mssCommand, "edit Monkey Skyway routes");
    registerCommand("npcr", 30, npcRotateCommand, "rotate NPCs");
    registerCommand("npci", 30, npcInstanceCommand, "move NPCs across instances");
    registerCommand("summonW", 30, summonWCommand, "permanently summon NPCs");
    registerCommand("unsummonW", 30, unsummonWCommand, "delete permanently summoned NPCs");
    registerCommand("toggleai", 30, toggleAiCommand, "enable/disable mob AI");
    registerCommand("flush", 30, flushCommand, "save gruntwork to file");
    registerCommand("level", 50, levelCommand, "change your character's level");
    registerCommand("levelx", 50, levelCommand, "change your character's level"); // for Academy
    registerCommand("population", 100, populationCommand, "check how many players are online");
    registerCommand("refresh", 100, refreshCommand, "teleport yourself to your current location");
    registerCommand("minfo", 30, minfoCommand, "show details of the current mission and task.");
    registerCommand("buff", 50, buffCommand, "give yourself a buff effect");
    registerCommand("egg", 30, eggCommand, "summon a coco egg");
    registerCommand("tasks", 30, tasksCommand, "list all active missions and their respective task ids.");
    registerCommand("notify", 30, notifyCommand, "receive a message whenever a player joins the server");
    registerCommand("players", 30, playersCommand, "print all players on the server");
    registerCommand("summonGroup", 30, summonGroupCommand, "summon group NPCs");
    registerCommand("summonGroupW", 30, summonGroupCommand, "permanently summon group NPCs");
    registerCommand("whois", 50, whoisCommand, "describe nearest NPC");
    registerCommand("lair", 50, lairUnlockCommand, "get the required mission for the nearest fusion lair");
    registerCommand("hide", 100, hideCommand, "hide yourself from the global player map");
    registerCommand("unhide", 100, unhideCommand, "un-hide yourself from the global player map");
    registerCommand("unwarpable", 100, unwarpableCommand, "prevent buddies from warping to you");
    registerCommand("warpable", 100, warpableCommand, "re-allow buddies to warp to you");
    registerCommand("redeem", 100, redeemCommand, "redeem a code item");
}

void ChatManager::registerCommand(std::string cmd, int requiredLevel, CommandHandler handlr, std::string help) {
    commands[cmd] = ChatCommand(requiredLevel, handlr, help);
}

void ChatManager::chatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_FREECHAT_MESSAGE*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    std::string fullChat = sanitizeText(U16toU8(chat->szFreeChat));
    if (fullChat.length() > 1 && fullChat[0] == CMD_PREFIX) { // PREFIX
        runCmd(fullChat, sock);
        return;
    }

    if (plr->iSpecialState & CN_SPECIAL_STATE_FLAG__MUTE_FREECHAT)
        return;

    std::string logLine = "[FreeChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iPC_ID = plr->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC));

    // send to visible players
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_FREECHAT_MESSAGE_SUCC));
}

void ChatManager::menuChatHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE))
        return; // malformed packet

    sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE* chat = (sP_CL2FE_REQ_SEND_MENUCHAT_MESSAGE*)data->buf;
    Player *plr = PlayerManager::getPlayer(sock);

    std::string fullChat = sanitizeText(U16toU8(chat->szFreeChat));
    std::string logLine = "[MenuChat] " + PlayerManager::getPlayerName(plr, true) + ": " + fullChat;

    std::cout << logLine << std::endl;
    dump.push_back(logLine);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, resp);

    U8toU16(fullChat, (char16_t*)&resp.szFreeChat, sizeof(resp.szFreeChat));
    resp.iPC_ID = PlayerManager::getPlayer(sock)->iID;
    resp.iEmoteCode = chat->iEmoteCode;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC));

    // send to visible players
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC, sizeof(sP_FE2CL_REP_SEND_MENUCHAT_MESSAGE_SUCC));
}

void ChatManager::emoteHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT))
        return; // ignore the malformed packet

    // you can dance with friends!!!!!!!!

    sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT* emote = (sP_CL2FE_REQ_PC_AVATAR_EMOTES_CHAT*)data->buf;
    Player* plr = PlayerManager::getPlayer(sock);

    // send to client
    INITSTRUCT(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, resp);
    resp.iEmoteCode = emote->iEmoteCode;
    resp.iID_From = plr->iID;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));

    // send to visible players (players within render distance)
    PlayerManager::sendToViewable(sock, (void*)&resp, P_FE2CL_REP_PC_AVATAR_EMOTES_CHAT, sizeof(sP_FE2CL_REP_PC_AVATAR_EMOTES_CHAT));
}

void ChatManager::sendServerMessage(CNSocket* sock, std::string msg) {
    INITSTRUCT(sP_FE2CL_PC_MOTD_LOGIN, motd);

    motd.iType = 1;
    // convert string to u16 and write it to the buffer
    U8toU16(msg, (char16_t*)motd.szSystemMsg, sizeof(motd.szSystemMsg));

    // send the packet :)
    sock->sendPacket((void*)&motd, P_FE2CL_PC_MOTD_LOGIN, sizeof(sP_FE2CL_PC_MOTD_LOGIN));
}

void ChatManager::announcementHandler(CNSocket* sock, CNPacketData* data) {
    if (data->size != sizeof(sP_CL2FE_GM_REQ_PC_ANNOUNCE))
        return; // ignore malformed packet

    Player* plr = PlayerManager::getPlayer(sock);
    if (plr->accountLevel > 30)
        return; // only players with account level less than 30 (GM) are allowed to use this command
    sP_CL2FE_GM_REQ_PC_ANNOUNCE* announcement = (sP_CL2FE_GM_REQ_PC_ANNOUNCE*)data->buf;

    INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
    msg.iAnnounceType = announcement->iAnnounceType;
    msg.iDuringTime = announcement->iDuringTime;
    memcpy(msg.szAnnounceMsg, announcement->szAnnounceMsg, sizeof(msg.szAnnounceMsg));
    std::map<CNSocket*, Player*>::iterator it;

    switch (announcement->iAreaType) {
    case 0: // area (all players in viewable chunks)
        sock->sendPacket((void*)&msg, P_FE2CL_GM_REP_PC_ANNOUNCE, sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE));
        PlayerManager::sendToViewable(sock, (void*)&msg, P_FE2CL_GM_REP_PC_ANNOUNCE, sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE));
        break;
    case 1: // shard
    case 2: // world
        break; // not applicable to OpenFusion
    case 3: // global (all players)
        for (it = PlayerManager::players.begin(); it != PlayerManager::players.end(); it++) {
            CNSocket* allSock = it->first;
            allSock->sendPacket((void*)&msg, P_FE2CL_GM_REP_PC_ANNOUNCE, sizeof(sP_FE2CL_GM_REP_PC_ANNOUNCE));
        }
    default:
        break;
    }

    std::string logLine = "[Bcast " + std::to_string(announcement->iAreaType) + "] " + PlayerManager::getPlayerName(plr, false) + ": " + U16toU8(msg.szAnnounceMsg);
    std::cout << logLine << std::endl;
    dump.push_back("**" + logLine + "**");
}

// we only allow plain ascii, at least for now
std::string ChatManager::sanitizeText(std::string text, bool allowNewlines) {
    int i;
    const int BUFSIZE = 512;
    char buf[BUFSIZE];

    assert(text.size() < BUFSIZE);

    i = 0;
    for (char c : text) {
        if (i >= BUFSIZE-1)
            break;

        if (!allowNewlines && c == '\n')
            continue;

        if ((c >= ' ' && c <= '~') || c == '\n')
            buf[i++] = c;
    }
    buf[i] = 0;

    return std::string(buf);
}
