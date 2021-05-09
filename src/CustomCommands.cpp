#include "CustomCommands.hpp"
#include "Chat.hpp"
#include "PlayerManager.hpp"
#include "TableData.hpp"
#include "NPCManager.hpp"
#include "Eggs.hpp"
#include "MobAI.hpp"
#include "Items.hpp"
#include "db/Database.hpp"
#include "Transport.hpp"
#include "Missions.hpp"

#include <sstream>
#include <iterator>
#include <math.h>

typedef void (*CommandHandler)(std::string fullString, std::vector<std::string>& args, CNSocket* sock);

struct ChatCommand {
    int requiredAccLevel;
    std::string help;
    CommandHandler handlr;

    ChatCommand(int r, CommandHandler h): requiredAccLevel(r), handlr(h) {}
    ChatCommand(int r, CommandHandler h, std::string str): requiredAccLevel(r), help(str), handlr(h) {}
    ChatCommand(): ChatCommand(0, nullptr) {}
};

static std::map<std::string, ChatCommand> commands;

static std::vector<std::string> parseArgs(std::string full) {
    std::stringstream ss(full);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    return std::vector<std::string>(begin, end);
}

bool CustomCommands::runCmd(std::string full, CNSocket* sock) {
    std::vector<std::string> args = parseArgs(full);
    std::string cmd = args[0].substr(1, args[0].size() - 1);

    // check if the command exists
    if (commands.find(cmd) != commands.end()) {
        Player* plr = PlayerManager::getPlayer(sock);
        ChatCommand command = commands[cmd];

        // sanity check + does the player have the required account level to use the command?
        if (plr != nullptr && plr->accountLevel <= command.requiredAccLevel) {
            command.handlr(full, args, sock);
            return true;
        } else {
            Chat::sendServerMessage(sock, "You don't have access to that command!");
            return false;
        }
    }

    Chat::sendServerMessage(sock, "Unknown command!");
    return false;
}

//

static void updatePathMarkers(CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (TableData::RunningNPCPaths.find(plr->iID) != TableData::RunningNPCPaths.end()) {
        for (BaseNPC* marker : TableData::RunningNPCPaths[plr->iID].second)
            marker->enterIntoViewOf(sock);
    }
}

//

static void helpCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Chat::sendServerMessage(sock, "Commands available to you:");
    Player *plr = PlayerManager::getPlayer(sock);

    for (auto& cmd : commands) {
        if (cmd.second.requiredAccLevel >= plr->accountLevel)
            Chat::sendServerMessage(sock, "/" + cmd.first + (cmd.second.help.length() > 0 ? " - " + cmd.second.help : ""));
    }
}

static void accessCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Chat::sendServerMessage(sock, "Your access level is " + std::to_string(PlayerManager::getPlayer(sock)->accountLevel));
}

static void populationCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Chat::sendServerMessage(sock, std::to_string(PlayerManager::players.size()) + " players online");
}

static void levelCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "/level: no level specified");
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

    sock->sendPacket(resp, P_FE2CL_REP_PC_CHANGE_LEVEL);
    PlayerManager::sendToViewable(sock, resp, P_FE2CL_REP_PC_CHANGE_LEVEL);
}

static void mssCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "[MSS] Too few arguments");
        Chat::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test/export> <<height>>");
        return;
    }

    // Validate route number
    char* routeNumC;
    int routeNum = std::strtol(args[1].c_str(), &routeNumC, 10);
    if (*routeNumC) {
        // not an integer
        Chat::sendServerMessage(sock, "[MSS] Invalid route number '" + args[1] + "'");
        return;
    }

    if (args.size() < 3) {
        Chat::sendServerMessage(sock, "[MSS] Too few arguments");
        Chat::sendServerMessage(sock, "[MSS] Usage: /mss <route> <add/remove/goto/clear/test> <<height>>");
        return;
    }

    // get the route (if it doesn't exist yet, this will also make it)
    std::vector<Vec3>* route = &TableData::RunningSkywayRoutes[routeNum];

    // mss <route> add <height>
    if (args[2] == "add") {
        // make sure height token exists
        if (args.size() < 4) {
            Chat::sendServerMessage(sock, "[MSS] Point height must be specified");
            Chat::sendServerMessage(sock, "[MSS] Usage: /mss <route> add <height>");
            return;
        }
        // validate height token
        char* heightC;
        int height = std::strtol(args[3].c_str(), &heightC, 10);
        if (*heightC) {
            Chat::sendServerMessage(sock, "[MSS] Invalid height " + args[3]);
            return;
        }

        Player* plr = PlayerManager::getPlayer(sock);
        route->push_back({ plr->x, plr->y, height }); // add point
        Chat::sendServerMessage(sock, "[MSS] Added point (" + std::to_string(plr->x) + ", " + std::to_string(plr->y) + ", " + std::to_string(height) + ") to route " + std::to_string(routeNum));
        return;
    }

    // mss <route> remove
    if (args[2] == "remove") {
        if (route->empty()) {
            Chat::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        Vec3 pulled = route->back();
        route->pop_back(); // remove point at top of stack
        Chat::sendServerMessage(sock, "[MSS] Removed point (" + std::to_string(pulled.x) + ", " + std::to_string(pulled.y) + ", " + std::to_string(pulled.z) + ") from route " + std::to_string(routeNum));
        return;
    }

    // mss <route> goto
    if (args[2] == "goto") {
        if (route->empty()) {
            Chat::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        Vec3 pulled = route->back();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        return;
    }

    // mss <route> clear
    if (args[2] == "clear") {
        route->clear();
        Chat::sendServerMessage(sock, "[MSS] Cleared route " + std::to_string(routeNum));
        return;
    }

    // mss <route> test
    if (args[2] == "test") {
        if (route->empty()) {
            Chat::sendServerMessage(sock, "[MSS] Route " + std::to_string(routeNum) + " is empty");
            return;
        }

        Vec3 pulled = route->front();
        PlayerManager::sendPlayerTo(sock, pulled.x, pulled.y, pulled.z);
        Transport::testMssRoute(sock, route);
        return;
    }

    // for compatibility: mss <route> export
    if (args[2] == "export") {
        Chat::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
        TableData::flush();
        return;
    }

    // mss ????
    Chat::sendServerMessage(sock, "[MSS] Unknown command '" + args[2] + "'");
}

static void summonWCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "/summonW: no mob type specified");
        return;
    }
    Player* plr = PlayerManager::getPlayer(sock);

    char *rest;
    int type = std::strtol(args[1].c_str(), &rest, 10);
    if (*rest) {
        Chat::sendServerMessage(sock, "Invalid NPC number: " + args[1]);
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

    Chat::sendServerMessage(sock, "/summonW: placed mob with type: " + std::to_string(type) +
        ", id: " + std::to_string(npc->appearanceData.iNPC_ID));
    TableData::RunningMobs[npc->appearanceData.iNPC_ID] = npc; // only record the one in the template
}

static void unsummonWCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC* npc = NPCManager::getNearestNPC(&plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        Chat::sendServerMessage(sock, "/unsummonW: No NPCs found nearby");
        return;
    }

    if (TableData::RunningEggs.find(npc->appearanceData.iNPC_ID) != TableData::RunningEggs.end()) {
        Chat::sendServerMessage(sock, "/unsummonW: removed egg with type: " + std::to_string(npc->appearanceData.iNPCType) +
            ", id: " + std::to_string(npc->appearanceData.iNPC_ID));
        TableData::RunningEggs.erase(npc->appearanceData.iNPC_ID);
        NPCManager::destroyNPC(npc->appearanceData.iNPC_ID);
        return;
    }

    if (TableData::RunningMobs.find(npc->appearanceData.iNPC_ID) == TableData::RunningMobs.end()
        && TableData::RunningGroups.find(npc->appearanceData.iNPC_ID) == TableData::RunningGroups.end()) {
        Chat::sendServerMessage(sock, "/unsummonW: Closest NPC is not a gruntwork mob.");
        return;
    }

    if (NPCManager::NPCs.find(npc->appearanceData.iNPC_ID) != NPCManager::NPCs.end() && NPCManager::NPCs[npc->appearanceData.iNPC_ID]->type == EntityType::MOB) {
        int leadId = ((Mob*)npc)->groupLeader;
        if (leadId != 0) {
            if (NPCManager::NPCs.find(leadId) == NPCManager::NPCs.end() || NPCManager::NPCs[leadId]->type != EntityType::MOB) {
                std::cout << "[WARN] unsummonW: leader not found!" << std::endl;
            }
            Mob* leadNpc = (Mob*)NPCManager::NPCs[leadId];
            for (int i = 0; i < 4; i++) {
                if (leadNpc->groupMember[i] == 0)
                    break;

                if (NPCManager::NPCs.find(leadNpc->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[leadNpc->groupMember[i]]->type != EntityType::MOB) {
                    std::cout << "[WARN] unsommonW: leader can't find a group member!" << std::endl;
                    continue;
                }

                NPCManager::destroyNPC(leadNpc->groupMember[i]);
            }
            TableData::RunningGroups.erase(leadId);
            NPCManager::destroyNPC(leadId);
            Chat::sendServerMessage(sock, "/unsummonW: Mob group destroyed.");
            return;
        }
    }

    Chat::sendServerMessage(sock, "/unsummonW: removed mob with type: " + std::to_string(npc->appearanceData.iNPCType) +
        ", id: " + std::to_string(npc->appearanceData.iNPC_ID));

    TableData::RunningMobs.erase(npc->appearanceData.iNPC_ID);

    NPCManager::destroyNPC(npc->appearanceData.iNPC_ID);
}

static void toggleAiCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    MobAI::simulateMobs = !MobAI::simulateMobs;

    if (MobAI::simulateMobs)
        return;

    // return all mobs to their spawn points
    for (auto& pair : NPCManager::NPCs) {
        if (pair.second->type != EntityType::MOB)
            continue;

        Mob* mob = (Mob*)pair.second;
        mob->state = MobState::RETREAT;
        mob->target = nullptr;
        mob->nextMovement = getTime();

        // mobs with static paths can chill where they are
        if (mob->staticPath) {
            mob->roamX = mob->x;
            mob->roamY = mob->y;
            mob->roamZ = mob->z;
        } else {
            mob->roamX = mob->spawnX;
            mob->roamY = mob->spawnY;
            mob->roamZ = mob->spawnZ;
        }
    }
}

static void npcRotateCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    BaseNPC* npc = NPCManager::getNearestNPC(&plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        Chat::sendServerMessage(sock, "[NPCR] No NPCs found nearby");
        return;
    }

    int angle = (plr->angle + 180) % 360;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->x, npc->y, npc->z, npc->instanceID, angle);

    // if it's a gruntwork NPC, rotate in-place
    if (TableData::RunningMobs.find(npc->appearanceData.iNPC_ID) != TableData::RunningMobs.end()) {
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->x, npc->y, npc->z, npc->instanceID, angle);

        Chat::sendServerMessage(sock, "[NPCR] Successfully set angle to " + std::to_string(angle) + " for gruntwork NPC "
            + std::to_string(npc->appearanceData.iNPC_ID));
    } else {
        TableData::RunningNPCRotations[npc->appearanceData.iNPC_ID] = angle;

        Chat::sendServerMessage(sock, "[NPCR] Successfully set angle to " + std::to_string(angle) + " for NPC "
            + std::to_string(npc->appearanceData.iNPC_ID));
    }

    // update rotation clientside
    INITSTRUCT(sP_FE2CL_NPC_ENTER, pkt);
    pkt.NPCAppearanceData = npc->appearanceData;
    sock->sendPacket(pkt, P_FE2CL_NPC_ENTER);
}

static void refreshCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    EntityRef ref = {sock};
    Entity* plr = ref.getEntity();
    ChunkPos currentChunk = plr->chunkPos;
    ChunkPos nullChunk = std::make_tuple(0, 0, 0);
    Chunking::updateEntityChunk(ref, currentChunk, nullChunk);
    Chunking::updateEntityChunk(ref, nullChunk, currentChunk);
}

static void instanceCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    // no additional arguments: report current instance ID
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "[INST] Current instance ID: " + std::to_string(plr->instanceID));
        Chat::sendServerMessage(sock, "[INST] (Map " + std::to_string(MAPNUM(plr->instanceID)) + ", instance " + std::to_string(PLAYERID(plr->instanceID)) + ")");
        return;
    }

    // move player to specified instance
    // validate instance ID
    char* instanceS;
    uint64_t instance = std::strtoll(args[1].c_str(), &instanceS, 10);
    if (*instanceS) {
        Chat::sendServerMessage(sock, "[INST] Invalid instance ID: " + args[1]);
        return;
    }

    if (args.size() >= 3) {
        char* playeridS;
        uint64_t playerid = std::strtoll(args[2].c_str(), &playeridS, 10);

        if (playerid != 0) {
            instance |= playerid << 32ULL;
            Chunking::createInstance(instance);

            // a precaution
            plr->recallInstance = 0;
        }
    }

    PlayerManager::sendPlayerTo(sock, plr->x, plr->y, plr->z, instance);
    Chat::sendServerMessage(sock, "[INST] Switched to instance with ID " + std::to_string(instance));
}

static void npcInstanceCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "[NPCI] Instance ID must be specified");
        Chat::sendServerMessage(sock, "[NPCI] Usage: /npci <instance ID>");
        return;
    }

    BaseNPC* npc = NPCManager::getNearestNPC(&plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        Chat::sendServerMessage(sock, "[NPCI] No NPCs found nearby");
        return;
    }

    // validate instance ID
    char* instanceS;
    int instance = std::strtol(args[1].c_str(), &instanceS, 10);
    if (*instanceS) {
        Chat::sendServerMessage(sock, "[NPCI] Invalid instance ID: " + args[1]);
        return;
    }

    Chat::sendServerMessage(sock, "[NPCI] Moving NPC with ID " + std::to_string(npc->appearanceData.iNPC_ID) + " to instance " + std::to_string(instance));
    TableData::RunningNPCMapNumbers[npc->appearanceData.iNPC_ID] = instance;
    NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->x, npc->y, npc->z, instance, npc->appearanceData.iAngle);
}

static void minfoCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    Chat::sendServerMessage(sock, "[MINFO] Current mission ID: " + std::to_string(plr->CurrentMissionID));

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] != 0) {
            TaskData& task = *Missions::Tasks[plr->tasks[i]];
            if ((int)(task["m_iHMissionID"]) == plr->CurrentMissionID) {
                Chat::sendServerMessage(sock, "[MINFO] Current task ID: " + std::to_string(plr->tasks[i]));
                Chat::sendServerMessage(sock, "[MINFO] Current task type: " + std::to_string((int)(task["m_iHTaskType"])));
                Chat::sendServerMessage(sock, "[MINFO] Current waypoint NPC ID: " + std::to_string((int)(task["m_iSTGrantWayPoint"])));
                Chat::sendServerMessage(sock, "[MINFO] Current terminator NPC ID: " + std::to_string((int)(task["m_iHTerminatorNPCID"])));

                if ((int)(task["m_iSTGrantTimer"]) != 0)
                    Chat::sendServerMessage(sock, "[MINFO] Current task timer: " + std::to_string((int)(task["m_iSTGrantTimer"])));

                for (int j = 0; j < 3; j++)
                    if ((int)(task["m_iCSUEnemyID"][j]) != 0)
                        Chat::sendServerMessage(sock, "[MINFO] Current task mob #" + std::to_string(j+1) +": " + std::to_string((int)(task["m_iCSUEnemyID"][j])));

                return;
            }
        }
    }
}

static void tasksCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {

    Player* plr = PlayerManager::getPlayer(sock);

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (plr->tasks[i] != 0) {
            TaskData& task = *Missions::Tasks[plr->tasks[i]];
            Chat::sendServerMessage(sock, "[TASK-" + std::to_string(i) + "] mission ID: " + std::to_string((int)(task["m_iHMissionID"])));
            Chat::sendServerMessage(sock, "[TASK-" + std::to_string(i) + "] task ID: " + std::to_string(plr->tasks[i]));
        }
    }
}

static void buffCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 3) {
        Chat::sendServerMessage(sock, "/buff: no skill Id and duration time specified");
        return;
    }

    char* tmp;
    int  skillId = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;
    int  duration = std::strtol(args[2].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if (Eggs::eggBuffPlayer(sock, skillId, 0, duration)<0)
        Chat::sendServerMessage(sock, "/buff: unknown skill Id");
    
}

static void eggCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "/egg: no egg type specified");
        return;
    }

    char* tmp;
    int eggType = std::strtol(args[1].c_str(), &tmp, 10);
    if (*tmp)
        return;

    if (Eggs::EggTypes.find(eggType) == Eggs::EggTypes.end()) {
        Chat::sendServerMessage(sock, "/egg: Unknown egg type");
        return;
    }

    //assert(NPCManager::nextId < INT32_MAX);
    int id = NPCManager::nextId--;

    Player* plr = PlayerManager::getPlayer(sock);

    // some math to place egg nicely in front of the player
    // temporarly disabled for sake of gruntwork
    int addX = 0; //-500.0f * sin(plr->angle / 180.0f * M_PI);
    int addY = 0;  //-500.0f * cos(plr->angle / 180.0f * M_PI);

    Egg* egg = new Egg(plr->x + addX, plr->y + addY, plr->z, plr->instanceID, eggType, id, false); // change last arg to true after gruntwork
    NPCManager::NPCs[id] = egg;
    NPCManager::updateNPCPosition(id, plr->x + addX, plr->y + addY, plr->z, plr->instanceID, plr->angle);

    // add to template
    TableData::RunningEggs[id] = egg;
}

static void notifyCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    if (plr->notify) {
        plr->notify = false;
        Chat::sendServerMessage(sock, "[ADMIN] No longer receiving join notifications");
    } else {
        plr->notify = true;
        Chat::sendServerMessage(sock, "[ADMIN] Receiving join notifications");
    }
}

static void playersCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Chat::sendServerMessage(sock, "[ADMIN] Players on the server:");
    for (auto pair : PlayerManager::players)
        Chat::sendServerMessage(sock, PlayerManager::getPlayerName(pair.second));
}

static void summonGroupCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 4) {
        Chat::sendServerMessage(sock, "/summonGroup(W) <leadermob> <mob> <number> [distance]");
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
        Chat::sendServerMessage(sock, "Invalid NPC number: " + args[1]);
        return;
    }

    int limit = NPCManager::NPCData.back()["m_iNpcNumber"];

    // permission & sanity check
    if (type > limit || type2 > limit || count > 5) {
        Chat::sendServerMessage(sock, "Invalid parameters; double check types and count");
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
        if (team == 2 && i > 0 && npc->type == EntityType::MOB) {
            leadNpc->groupMember[i-1] = npc->appearanceData.iNPC_ID;
            Mob* mob = (Mob*)NPCManager::NPCs[npc->appearanceData.iNPC_ID];
            mob->groupLeader = leadNpc->appearanceData.iNPC_ID;
            mob->offsetX = x - plr->x;
            mob->offsetY = y - plr->y;
        }

        npc->appearanceData.iAngle = (plr->angle + 180) % 360;
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, x, y, z, plr->instanceID, npc->appearanceData.iAngle);

        // if we're in a lair, we need to spawn the NPC in both the private instance and the template
        if (PLAYERID(plr->instanceID) != 0) {
            npc = NPCManager::summonNPC(plr->x, plr->y, plr->z, plr->instanceID, type, wCommand, true);

            if (team == 2 && i > 0 && npc->type == EntityType::MOB) {
                leadNpc->groupMember[i-1] = npc->appearanceData.iNPC_ID;
                Mob* mob = (Mob*)NPCManager::NPCs[npc->appearanceData.iNPC_ID];
                mob->groupLeader = leadNpc->appearanceData.iNPC_ID;
                mob->offsetX = x - plr->x;
                mob->offsetY = y - plr->y;
            }

            npc->appearanceData.iAngle = (plr->angle + 180) % 360;
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, x, y, z, plr->instanceID, npc->appearanceData.iAngle);
        }

        Chat::sendServerMessage(sock, "/summonGroup(W): placed mob with type: " + std::to_string(type) +
            ", id: " + std::to_string(npc->appearanceData.iNPC_ID));

        if (i == 0 && team == 2 && npc->type == EntityType::MOB) {
            type = type2;
            leadNpc = (Mob*)NPCManager::NPCs[npc->appearanceData.iNPC_ID];
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

static void flushCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    TableData::flush();
    Chat::sendServerMessage(sock, "Wrote gruntwork to " + settings::GRUNTWORKJSON);
}

static void whoisCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    BaseNPC* npc = NPCManager::getNearestNPC(&plr->viewableChunks, plr->x, plr->y, plr->z);

    if (npc == nullptr) {
        Chat::sendServerMessage(sock, "[WHOIS] No NPCs found nearby");
        return;
    }

    Chat::sendServerMessage(sock, "[WHOIS] ID: " + std::to_string(npc->appearanceData.iNPC_ID));
    Chat::sendServerMessage(sock, "[WHOIS] Type: " + std::to_string(npc->appearanceData.iNPCType));
    Chat::sendServerMessage(sock, "[WHOIS] HP: " + std::to_string(npc->appearanceData.iHP));
    Chat::sendServerMessage(sock, "[WHOIS] CBF: " + std::to_string(npc->appearanceData.iConditionBitFlag));
    Chat::sendServerMessage(sock, "[WHOIS] EntityType: " + std::to_string((int)npc->type));
    Chat::sendServerMessage(sock, "[WHOIS] X: " + std::to_string(npc->x));
    Chat::sendServerMessage(sock, "[WHOIS] Y: " + std::to_string(npc->y));
    Chat::sendServerMessage(sock, "[WHOIS] Z: " + std::to_string(npc->z));
    Chat::sendServerMessage(sock, "[WHOIS] Angle: " + std::to_string(npc->appearanceData.iAngle));
    std::string chunkPosition = std::to_string(std::get<0>(npc->chunkPos)) + ", " + std::to_string(std::get<1>(npc->chunkPos)) + ", " + std::to_string(std::get<2>(npc->chunkPos));
    Chat::sendServerMessage(sock, "[WHOIS] Chunk: {" + chunkPosition + "}");
    Chat::sendServerMessage(sock, "[WHOIS] MapNum: " + std::to_string(MAPNUM(npc->instanceID)));
    Chat::sendServerMessage(sock, "[WHOIS] Instance: " + std::to_string(PLAYERID(npc->instanceID)));
}

static void lairUnlockCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (!Chunking::chunkExists(plr->chunkPos))
        return;

    Chunk* chnk = Chunking::chunks[plr->chunkPos];
    int taskID = -1;
    int missionID = -1;
    int found = 0;
    for (const EntityRef& ref : chnk->entities) {
        if (ref.type == EntityType::PLAYER)
            continue;

        int32_t id = ref.id;
        if (NPCManager::NPCs.find(id) == NPCManager::NPCs.end())
            continue;

        BaseNPC* npc = NPCManager::NPCs[id];
        for (auto it = NPCManager::Warps.begin(); it != NPCManager::Warps.end(); it++) {
            if ((*it).second.npcID == npc->appearanceData.iNPCType) {
                taskID = (*it).second.limitTaskID;
                missionID = Missions::Tasks[taskID]->task["m_iHMissionID"];
                found++;
                break;
            }
        }
    }

    if (missionID == -1 || taskID == -1) {
        Chat::sendServerMessage(sock, "You are NOT standing near a lair portal; move around and try again!");
        return;
    }

    if (found > 1) {
        Chat::sendServerMessage(sock, "More than one lair found; decrease chunk size and try again!");
        return;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TASK_START_SUCC, taskResp);
    Missions::startTask(plr, taskID);
    taskResp.iTaskNum = taskID;
    taskResp.iRemainTime = 0;
    sock->sendPacket(taskResp, P_FE2CL_REP_PC_TASK_START_SUCC);

    INITSTRUCT(sP_FE2CL_REP_PC_SET_CURRENT_MISSION_ID, missionResp);
    missionResp.iCurrentMissionID = missionID;
    plr->CurrentMissionID = missionID;
    sock->sendPacket(missionResp, P_FE2CL_REP_PC_SET_CURRENT_MISSION_ID);
}

static void hideCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (plr->hidden) {
        Chat::sendServerMessage(sock, "[HIDE] You're already hidden from the map.");
        return;
    }

    plr->hidden = true;
    Chat::sendServerMessage(sock, "[HIDE] Successfully hidden from the map.");
}

static void unhideCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);
    if (!plr->hidden) {
        Chat::sendServerMessage(sock, "[HIDE] You're already visible from the map.");
        return;
    }

    plr->hidden = false;
    Chat::sendServerMessage(sock, "[HIDE] Successfully un-hidden from the map.");
}

static void redeemCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "/redeem: No code specified");
        return;
    }

    // convert string to all lowercase
    const char* codeRaw = args[1].c_str();
    if (args[1].size() > 256) { // prevent overflow
        Chat::sendServerMessage(sock, "/redeem: Code too long");
        return;
    }

    char buf[256];
    for (int i = 0; i < args[1].size(); i++)
        buf[i] = std::tolower(codeRaw[i]);
    std::string code(buf, args[1].size());

    if (Items::CodeItems.find(code) == Items::CodeItems.end()) {
        Chat::sendServerMessage(sock, "/redeem: Unknown code");
        return;
    }

    Player* plr = PlayerManager::getPlayer(sock);

    if (Database::isCodeRedeemed(plr->iID, code)) {
        Chat::sendServerMessage(sock, "/redeem: You have already redeemed this code item");
        return;
    }

    int itemCount = Items::CodeItems[code].size();
    std::vector<int> slots;

    for (int i = 0; i < itemCount; i++) {
        slots.push_back(Items::findFreeSlot(plr));
        if (slots[i] == -1) {
            Chat::sendServerMessage(sock, "/redeem: Not enough space in inventory");

            // delete any temp items we might have set
            for (int j = 0; j < i; j++) {
                plr->Inven[slots[j]] = { 0, 0, 0, 0 }; // empty
            }
            return;
        }

        plr->Inven[slots[i]] = { 999, 999, 999, 0 }; // temp item; overwritten later
    }
    
    Database::recordCodeRedemption(plr->iID, code);

    for (int i = 0; i < itemCount; i++) {
        std::pair<int32_t, int32_t> item = Items::CodeItems[code][i];
        INITSTRUCT(sP_FE2CL_REP_PC_GIVE_ITEM_SUCC, resp);

        resp.eIL = 1;
        resp.iSlotNum = slots[i];
        resp.Item.iID = item.first;
        resp.Item.iType = item.second;
        // I think it is safe? :eyes
        resp.Item.iOpt = 1;

        // save serverside
        plr->Inven[resp.iSlotNum] = resp.Item;

        sock->sendPacket(resp, P_FE2CL_REP_PC_GIVE_ITEM_SUCC);
    }
    std::string msg = itemCount == 1 ? "You have redeemed a code item" : "You have redeemed code items";
    Chat::sendServerMessage(sock, msg);
}

static void unwarpableCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);
    plr->unwarpable = true;
}

static void warpableCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);
    plr->unwarpable = false;
}

static void registerallCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->iWarpLocationFlag = UINT32_MAX;
    plr->aSkywayLocationFlag[0] = UINT64_MAX;
    plr->aSkywayLocationFlag[1] = UINT64_MAX;

    // update the client
    INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, resp);

    resp.iWarpLocationFlag = plr->iWarpLocationFlag;
    resp.aWyvernLocationFlag[0] = plr->aSkywayLocationFlag[0];
    resp.aWyvernLocationFlag[1] = plr->aSkywayLocationFlag[1];

    sock->sendPacket(resp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC);
}

static void unregisterallCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player *plr = PlayerManager::getPlayer(sock);

    plr->iWarpLocationFlag = 0;
    plr->aSkywayLocationFlag[0] = 0;
    plr->aSkywayLocationFlag[1] = 0;

    // update the client
    INITSTRUCT(sP_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC, resp);

    resp.iWarpLocationFlag = plr->iWarpLocationFlag;
    resp.aWyvernLocationFlag[0] = plr->aSkywayLocationFlag[0];
    resp.aWyvernLocationFlag[1] = plr->aSkywayLocationFlag[1];

    sock->sendPacket(resp, P_FE2CL_REP_PC_REGIST_TRANSPORTATION_LOCATION_SUCC);
}

static void banCommand(std::string full, std::vector<std::string>& args, CNSocket *sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "Usage: /ban PlayerID [reason...]");
        return;
    }

    char *rest;
    int playerId = std::strtol(args[1].c_str(), &rest, 10);
    if (*rest) {
        Chat::sendServerMessage(sock, "Invalid PlayerID: " + args[1]);
        return;
    }

    std::string reason;
    if (args.size() == 2) {
        reason = "no reason given";
    } else {
        reason = args[2];
        for (int i = 3; i < args.size(); i++)
            reason += " " + args[i];
    }

    // ban the account that player belongs to
    if (!Database::banPlayer(playerId, reason)) {
        // propagating a more descriptive error message from banPlayer() would be too much work
        Chat::sendServerMessage(sock, "Failed to ban target player. Check server logs.");
        return;
    }

    Chat::sendServerMessage(sock, "Banned target player.");
    std::cout << "[INFO] " << PlayerManager::getPlayerName(plr) << " banned player " << playerId << std::endl;

    // if the player is online, kick them
    CNSocket *otherSock = PlayerManager::getSockFromID(playerId);
    if (otherSock == nullptr) {
        Chat::sendServerMessage(sock, "Player wasn't online. Didn't need to kick.");
        return;
    }

    Player *otherPlr = PlayerManager::getPlayer(otherSock);

    INITSTRUCT(sP_FE2CL_REP_PC_EXIT_SUCC, pkt);

    pkt.iID = otherPlr->iID;
    pkt.iExitCode = 3; // "a GM has terminated your connection"

    // send to target player
    otherSock->sendPacket(pkt, P_FE2CL_REP_PC_EXIT_SUCC);

    // ensure that the connection has terminated
    otherSock->kill();

    Chat::sendServerMessage(sock, PlayerManager::getPlayerName(otherPlr) + " was online. Kicked.");
}

static void unbanCommand(std::string full, std::vector<std::string>& args, CNSocket *sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "Usage: /unban PlayerID");
        return;
    }

    char *rest;
    int playerId = std::strtol(args[1].c_str(), &rest, 10);
    if (*rest) {
        Chat::sendServerMessage(sock, "Invalid PlayerID: " + args[1]);
        return;
    }

    if (Database::unbanPlayer(playerId)) {
        Chat::sendServerMessage(sock, "Unbanned player.");
        std::cout << "[INFO] " << PlayerManager::getPlayerName(plr) << " unbanned player " << playerId << std::endl;
    } else {
        Chat::sendServerMessage(sock, "Failed to unban player. Check server logs.");
    }
}

static void pathCommand(std::string full, std::vector<std::string>& args, CNSocket* sock) {
    Player* plr = PlayerManager::getPlayer(sock);

    if (args.size() < 2) {
        Chat::sendServerMessage(sock, "[PATH] Too few arguments");
        Chat::sendServerMessage(sock, "[PATH] Usage: /path <start/kf/undo/here/test/cancel/end>");
        updatePathMarkers(sock);
        return;
    }

    // /path start
    if (args[1] == "start") {
        // make sure the player doesn't have a follower
        if (TableData::RunningNPCPaths.find(plr->iID) != TableData::RunningNPCPaths.end()) {
            Chat::sendServerMessage(sock, "[PATH] An NPC is already following you");
            Chat::sendServerMessage(sock, "[PATH] Run '/path end' first, if you're done");
            updatePathMarkers(sock);
            return;
        }

        // find nearest NPC
        BaseNPC* npc = NPCManager::getNearestNPC(&plr->viewableChunks, plr->x, plr->y, plr->z);
        if (npc == nullptr) {
            Chat::sendServerMessage(sock, "[PATH] No NPCs found nearby");
            return;
        }

        // add first point at NPC's current location
        std::vector<BaseNPC*> pathPoints;
        BaseNPC* marker = new BaseNPC(npc->x, npc->y, npc->z, 0, plr->instanceID, 1386, NPCManager::nextId--);
        pathPoints.push_back(marker);
        // map from player
        TableData::RunningNPCPaths[plr->iID] = std::make_pair(npc, pathPoints);
        Chat::sendServerMessage(sock, "[PATH] NPC " + std::to_string(npc->appearanceData.iNPC_ID) + " is now following you");
        updatePathMarkers(sock);
        return;
    }

    // make sure the player has a follower
    if (TableData::RunningNPCPaths.find(plr->iID) == TableData::RunningNPCPaths.end()) {
        Chat::sendServerMessage(sock, "[PATH] No NPC is currently following you");
        Chat::sendServerMessage(sock, "[PATH] Run '/path start' near an NPC first");
        return;
    }

    std::pair<BaseNPC*, std::vector<BaseNPC*>>* entry = &TableData::RunningNPCPaths[plr->iID];
    BaseNPC* npc = entry->first;

    // /path kf
    if (args[1] == "kf") {
        BaseNPC* marker = new BaseNPC(npc->x, npc->y, npc->z, 0, plr->instanceID, 1386, NPCManager::nextId--);
        entry->second.push_back(marker);
        Chat::sendServerMessage(sock, "[PATH] Added keyframe");
        updatePathMarkers(sock);
        return;
    }

    // /path here
    if (args[1] == "here") {
        // bring the NPC to where the player is standing
        Transport::NPCQueues.erase(npc->appearanceData.iNPC_ID); // delete transport queue
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, plr->x, plr->y, plr->z, npc->instanceID, 0);
        npc->disappearFromViewOf(sock);
        npc->enterIntoViewOf(sock);
        Chat::sendServerMessage(sock, "[PATH] Come here");
        return;
    }

    // /path undo
    if (args[1] == "undo") {
        if (entry->second.size() == 1) {
            Chat::sendServerMessage(sock, "[PATH] Nothing to undo");
            return;
        }

        BaseNPC* marker = entry->second.back();
        marker->disappearFromViewOf(sock); // vanish
        delete marker; // destroy

        entry->second.pop_back(); // remove from the vector
        Chat::sendServerMessage(sock, "[PATH] Undid last keyframe");
        updatePathMarkers(sock);
        return;
    }

    // /path test
    if (args[1] == "test") {

        int speed = NPC_DEFAULT_SPEED;
        if (args.size() > 2) { // speed specified
            char* buf;
            int speedArg = std::strtol(args[2].c_str(), &buf, 10);
            if (*buf) {
                Chat::sendServerMessage(sock, "[PATH] Invalid speed " + args[2]);
                return;
            }
            speed = speedArg;
        }
        // return NPC to home
        Transport::NPCQueues.erase(npc->appearanceData.iNPC_ID); // delete transport queue
        BaseNPC* home = entry->second[0];
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, home->x, home->y, home->z, npc->instanceID, 0);
        npc->disappearFromViewOf(sock);
        npc->enterIntoViewOf(sock);

        // do lerping magic
        entry->second.push_back(home); // temporary end point for loop completion
        std::queue<Vec3> keyframes;
        auto _point = entry->second.begin();
        Vec3 from = { (*_point)->x, (*_point)->y, (*_point)->z }; // point A coords
        for (_point++; _point != entry->second.end(); _point++) { // loop through all point Bs
            Vec3 to = { (*_point)->x, (*_point)->y, (*_point)->z }; // point B coords
            // add point A to the queue
            keyframes.push(from);
            Transport::lerp(&keyframes, from, to, speed); // lerp from A to B
            from = to; // update point A
        }
        Transport::NPCQueues[npc->appearanceData.iNPC_ID] = keyframes;
        entry->second.pop_back(); // remove temp end point

        Chat::sendServerMessage(sock, "[PATH] Testing NPC path");
        return;
    }

    // /path cancel
    if (args[1] == "cancel") {
        // return NPC to home
        Transport::NPCQueues.erase(npc->appearanceData.iNPC_ID); // delete transport queue
        BaseNPC* home = entry->second[0];
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, home->x, home->y, home->z, npc->instanceID, 0);
        npc->disappearFromViewOf(sock);
        npc->enterIntoViewOf(sock);
        // deallocate markers
        for (BaseNPC* marker : entry->second) {
            marker->disappearFromViewOf(sock); // poof
            delete marker; // destroy
        }
        // unmap
        TableData::RunningNPCPaths.erase(plr->iID);
        Chat::sendServerMessage(sock, "[PATH] NPC " + std::to_string(npc->appearanceData.iNPC_ID) + " is no longer following you");
        return;
    }

    // /path end
    if (args[1] == "end") {
        if (args.size() < 2) {
            Chat::sendServerMessage(sock, "[PATH] Too few arguments");
            Chat::sendServerMessage(sock, "[PATH] Usage: /path end [speed] [a/r]");
            return;
        }

        int speed = NPC_DEFAULT_SPEED;
        bool relative = false;
        if (args.size() > 2) { // speed specified
            char* buf;
            int speedArg = std::strtol(args[2].c_str(), &buf, 10);
            if (*buf) {
                Chat::sendServerMessage(sock, "[PATH] Invalid speed " + args[2]);
                return;
            }
            speed = speedArg;
        }
        if (args.size() > 3 && args[3] == "r") { // relativity specified
            relative = true;
        }

        // return NPC to home and set path to repeat
        Transport::NPCQueues.erase(npc->appearanceData.iNPC_ID); // delete transport queue
        BaseNPC* home = entry->second[0];
        NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, home->x, home->y, home->z, npc->instanceID, 0);
        npc->disappearFromViewOf(sock);
        npc->enterIntoViewOf(sock);
        npc->loopingPath = true;

        // do lerping magic
        entry->second.push_back(home); // temporary end point for loop completion
        std::queue<Vec3> keyframes;
        auto _point = entry->second.begin();
        Vec3 from = { (*_point)->x, (*_point)->y, (*_point)->z }; // point A coords
        for (_point++; _point != entry->second.end(); _point++) { // loop through all point Bs
            Vec3 to = { (*_point)->x, (*_point)->y, (*_point)->z }; // point B coords
            // add point A to the queue
            keyframes.push(from);
            Transport::lerp(&keyframes, from, to, speed); // lerp from A to B
            from = to; // update point A
        }
        Transport::NPCQueues[npc->appearanceData.iNPC_ID] = keyframes;
        entry->second.pop_back(); // remove temp end point

        // save to gruntwork
        std::vector<Vec3> finalPoints;
        for (BaseNPC* marker : entry->second) {
            Vec3 coords = { marker->x, marker->y, marker->z };
            if (relative) {
                coords.x -= home->x;
                coords.y -= home->y;
                coords.z -= home->z;
            }
            finalPoints.push_back(coords);
        }

        // end point to complete the circuit
        if (relative)
            finalPoints.push_back({ 0, 0, 0 });
        else
            finalPoints.push_back({ home->x, home->y, home->z });

        NPCPath finishedPath;
        finishedPath.escortTaskID = -1;
        finishedPath.isRelative = relative;
        finishedPath.isLoop = true;
        finishedPath.speed = speed;
        finishedPath.points = finalPoints;
        finishedPath.targetIDs.push_back(npc->appearanceData.iNPC_ID);

        TableData::FinishedNPCPaths.push_back(finishedPath);

        // deallocate markers
        for (BaseNPC* marker : entry->second) {
            marker->disappearFromViewOf(sock); // poof
            delete marker; // destroy
        }
        // unmap
        TableData::RunningNPCPaths.erase(plr->iID);

        Chat::sendServerMessage(sock, "[PATH] NPC " + std::to_string(npc->appearanceData.iNPC_ID) + " is no longer following you");

        TableData::flush();
        Chat::sendServerMessage(sock, "[PATH] Path saved to gruntwork");
        return;
    }

    // /path ???
    Chat::sendServerMessage(sock, "[PATH] Unknown argument '" + args[1] + "'");
}

static void registerCommand(std::string cmd, int requiredLevel, CommandHandler handlr, std::string help) {
    commands[cmd] = ChatCommand(requiredLevel, handlr, help);
}

void CustomCommands::init() {
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
    registerCommand("ban", 30, banCommand, "ban the account the given PlayerID belongs to");
    registerCommand("unban", 30, unbanCommand, "unban the account the given PlayerID belongs to");
    registerCommand("whois", 50, whoisCommand, "describe nearest NPC");
    registerCommand("lair", 50, lairUnlockCommand, "get the required mission for the nearest fusion lair");
    registerCommand("hide", 100, hideCommand, "hide yourself from the global player map");
    registerCommand("unhide", 100, unhideCommand, "un-hide yourself from the global player map");
    registerCommand("unwarpable", 100, unwarpableCommand, "prevent buddies from warping to you");
    registerCommand("warpable", 100, warpableCommand, "re-allow buddies to warp to you");
    registerCommand("registerall", 50, registerallCommand, "register all SCAMPER and MSS destinations");
    registerCommand("unregisterall", 50, unregisterallCommand, "clear all SCAMPER and MSS destinations");
    registerCommand("redeem", 100, redeemCommand, "redeem a code item");
    registerCommand("path", 30, pathCommand, "edit NPC paths");
}
