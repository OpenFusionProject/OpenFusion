#include "ChunkManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"

std::map<ChunkPos, Chunk*> ChunkManager::chunks;

void ChunkManager::init() {} // stubbed

void ChunkManager::newChunk(ChunkPos pos) {
    if (chunkExists(pos)) {
        std::cout << "[WARN] Tried to create a chunk that already exists\n";
        return;
    }

    Chunk *chunk = new Chunk();

    chunk->players = std::set<CNSocket*>();
    chunk->NPCs = std::set<int32_t>();

    chunks[pos] = chunk;

    // add the chunk to the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for (Chunk* c : surroundings) {
        for (CNSocket* sock : c->players)
            PlayerManager::getPlayer(sock)->viewableChunks->insert(chunk);
        for (int32_t id : c->NPCs)
            NPCManager::NPCs[id]->viewableChunks->insert(chunk);
    }
}

void ChunkManager::deleteChunk(ChunkPos pos) {
    if (!chunkExists(pos)) {
        std::cout << "[WARN] Tried to delete a chunk that doesn't exist\n";
        return;
    }

    Chunk* chunk = chunks[pos];

    // remove the chunk from the cache of all players and NPCs in the surrounding chunks
    std::set<Chunk*> surroundings = getViewableChunks(pos);
    for(Chunk* c : surroundings)
    {
        for (CNSocket* sock : c->players)
            PlayerManager::getPlayer(sock)->viewableChunks->erase(chunk);
        for (int32_t id : c->NPCs)
            NPCManager::NPCs[id]->viewableChunks->erase(chunk);
    }

    chunks.erase(pos); // remove from map
    delete chunk; // free from memory
}

void ChunkManager::updatePlayerChunk(CNSocket* sock, ChunkPos from, ChunkPos to) {
    Player* plr = PlayerManager::getPlayer(sock);

    // if the new chunk doesn't exist, make it first
    if (!ChunkManager::chunkExists(to))
        newChunk(to);

    // move to other chunk's player set
    untrackPlayer(from, sock); // this will delete the chunk if it's empty
    trackPlayer(to, sock);

    // calculate viewable chunks from both points
    std::set<Chunk*> oldViewables = getViewableChunks(from);
    std::set<Chunk*> newViewables = getViewableChunks(to);
    std::set<Chunk*> toExit, toEnter;

    /*
     * Calculate diffs. This is done to prevent phasing on chunk borders.
     * toExit will contain old viewables - new viewables, so the player will only be exited in chunks that are out of sight.
     * toEnter contains the opposite: new viewables - old viewables, chunks where we previously weren't visible from before.
     */
    std::set_difference(oldViewables.begin(), oldViewables.end(), newViewables.begin(), newViewables.end(),
        std::inserter(toExit, toExit.end())); // chunks we must be EXITed from (old - new)
    std::set_difference(newViewables.begin(), newViewables.end(), oldViewables.begin(), oldViewables.end(),
        std::inserter(toEnter, toEnter.end())); // chunks we must be ENTERed into (new - old)

    // update views
    removePlayerFromChunks(toExit, sock);
    addPlayerToChunks(toEnter, sock);

    plr->chunkPos = to; // update cached chunk position
    // updated cached viewable chunks
    plr->viewableChunks->clear();
    plr->viewableChunks->insert(newViewables.begin(), newViewables.end());
}

void ChunkManager::updateNPCChunk(int32_t id, ChunkPos from, ChunkPos to) {
    BaseNPC* npc = NPCManager::NPCs[id];

    // if the new chunk doesn't exist, make it first
    if (!ChunkManager::chunkExists(to))
        newChunk(to);

    // move to other chunk's player set
    untrackNPC(from, id); // this will delete the chunk if it's empty
    trackNPC(to, id);

    // calculate viewable chunks from both points
    std::set<Chunk*> oldViewables = getViewableChunks(from);
    std::set<Chunk*> newViewables = getViewableChunks(to);
    std::set<Chunk*> toExit, toEnter;

    /*
     * Calculate diffs. This is done to prevent phasing on chunk borders.
     * toExit will contain old viewables - new viewables, so the player will only be exited in chunks that are out of sight.
     * toEnter contains the opposite: new viewables - old viewables, chunks where we previously weren't visible from before.
     */
    std::set_difference(oldViewables.begin(), oldViewables.end(), newViewables.begin(), newViewables.end(),
        std::inserter(toExit, toExit.end())); // chunks we must be EXITed from (old - new)
    std::set_difference(newViewables.begin(), newViewables.end(), oldViewables.begin(), oldViewables.end(),
        std::inserter(toEnter, toEnter.end())); // chunks we must be ENTERed into (new - old)

    // update views
    removeNPCFromChunks(toExit, id);
    addNPCToChunks(toEnter, id);

    npc->chunkPos = to; // update cached chunk position
    // updated cached viewable chunks
    npc->viewableChunks->clear();
    npc->viewableChunks->insert(newViewables.begin(), newViewables.end());
}

void ChunkManager::trackPlayer(ChunkPos chunkPos, CNSocket* sock) {
    if (!chunkExists(chunkPos))
        return; // shouldn't happen

    chunks[chunkPos]->players.insert(sock);
}

void ChunkManager::trackNPC(ChunkPos chunkPos, int32_t id) {
    if (!chunkExists(chunkPos))
        return; // shouldn't happen

    chunks[chunkPos]->NPCs.insert(id);
}

void ChunkManager::untrackPlayer(ChunkPos chunkPos, CNSocket* sock) {
    if (!chunkExists(chunkPos))
        return; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->players.erase(sock); // gone

    // if chunk is empty, free it
    if (chunk->NPCs.size() == 0 && chunk->players.size() == 0)
        deleteChunk(chunkPos);
}

void ChunkManager::untrackNPC(ChunkPos chunkPos, int32_t id) {
    if (!chunkExists(chunkPos))
        return; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->NPCs.erase(id); // gone

    // if chunk is empty, free it
    if (chunk->NPCs.size() == 0 && chunk->players.size() == 0)
        deleteChunk(chunkPos);
}

void ChunkManager::addPlayerToChunks(std::set<Chunk*> chnks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_PC_NEW, newPlayer);

    for (Chunk* chunk : chnks) {
        // add npcs
        for (int32_t id : chunk->NPCs) {
            BaseNPC* npc = NPCManager::NPCs[id];

            if (npc->appearanceData.iHP <= 0)
                continue;

            switch (npc->npcClass) {
            case NPC_BUS:
                INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, enterBusData);
                enterBusData.AppearanceData = { 3, npc->appearanceData.iNPC_ID, npc->appearanceData.iNPCType, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ };
                sock->sendPacket((void*)&enterBusData, P_FE2CL_TRANSPORTATION_ENTER, sizeof(sP_FE2CL_TRANSPORTATION_ENTER));
                break;
            case NPC_EGG:
                INITSTRUCT(sP_FE2CL_SHINY_ENTER, enterEggData);
                NPCManager::npcDataToEggData(&npc->appearanceData, &enterEggData.ShinyAppearanceData);
                sock->sendPacket((void*)&enterEggData, P_FE2CL_SHINY_ENTER, sizeof(sP_FE2CL_SHINY_ENTER));
                break;
            default:
                INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
                enterData.NPCAppearanceData = NPCManager::NPCs[id]->appearanceData;
                sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
                break;
            }
        }

        // add players
        for (CNSocket* otherSock : chunk->players) {
            if (sock == otherSock)
                continue; // that's us :P

            Player* otherPlr = PlayerManager::getPlayer(otherSock);
            Player* plr = PlayerManager::getPlayer(sock);

            newPlayer.PCAppearanceData.iID = plr->iID;
            newPlayer.PCAppearanceData.iHP = plr->HP;
            newPlayer.PCAppearanceData.iLv = plr->level;
            newPlayer.PCAppearanceData.iX = plr->x;
            newPlayer.PCAppearanceData.iY = plr->y;
            newPlayer.PCAppearanceData.iZ = plr->z;
            newPlayer.PCAppearanceData.iAngle = plr->angle;
            newPlayer.PCAppearanceData.PCStyle = plr->PCStyle;
            newPlayer.PCAppearanceData.Nano = plr->Nanos[plr->activeNano];
            newPlayer.PCAppearanceData.iPCState = plr->iPCState;
            newPlayer.PCAppearanceData.iSpecialState = plr->iSpecialState;
            memcpy(newPlayer.PCAppearanceData.ItemEquip, plr->Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            otherSock->sendPacket((void*)&newPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW));

            newPlayer.PCAppearanceData.iID = otherPlr->iID;
            newPlayer.PCAppearanceData.iHP = otherPlr->HP;
            newPlayer.PCAppearanceData.iLv = otherPlr->level;
            newPlayer.PCAppearanceData.iX = otherPlr->x;
            newPlayer.PCAppearanceData.iY = otherPlr->y;
            newPlayer.PCAppearanceData.iZ = otherPlr->z;
            newPlayer.PCAppearanceData.iAngle = otherPlr->angle;
            newPlayer.PCAppearanceData.PCStyle = otherPlr->PCStyle;
            newPlayer.PCAppearanceData.Nano = otherPlr->Nanos[otherPlr->activeNano];
            newPlayer.PCAppearanceData.iPCState = otherPlr->iPCState;
            newPlayer.PCAppearanceData.iSpecialState = otherPlr->iSpecialState;
            memcpy(newPlayer.PCAppearanceData.ItemEquip, otherPlr->Equip, sizeof(sItemBase) * AEQUIP_COUNT);

            sock->sendPacket((void*)&newPlayer, P_FE2CL_PC_NEW, sizeof(sP_FE2CL_PC_NEW));
        }
    }
}

void ChunkManager::addNPCToChunks(std::set<Chunk*> chnks, int32_t id) {
    BaseNPC* npc = NPCManager::NPCs[id];

    switch (npc->npcClass) {
    case NPC_BUS:
        INITSTRUCT(sP_FE2CL_TRANSPORTATION_ENTER, enterBusData);
        enterBusData.AppearanceData = { 3, npc->appearanceData.iNPC_ID, npc->appearanceData.iNPCType, npc->appearanceData.iX, npc->appearanceData.iY, npc->appearanceData.iZ };

        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterBusData, P_FE2CL_TRANSPORTATION_ENTER, sizeof(sP_FE2CL_TRANSPORTATION_ENTER));
            }
        }
        break;
    case NPC_EGG:
        INITSTRUCT(sP_FE2CL_SHINY_ENTER, enterEggData);
        NPCManager::npcDataToEggData(&npc->appearanceData, &enterEggData.ShinyAppearanceData);

        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterEggData, P_FE2CL_SHINY_ENTER, sizeof(sP_FE2CL_SHINY_ENTER));
            }
        }
        break;
    default:
        // create struct
        INITSTRUCT(sP_FE2CL_NPC_ENTER, enterData);
        enterData.NPCAppearanceData = npc->appearanceData;

        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER));
            }
        }
        break;
    }
}

void ChunkManager::removePlayerFromChunks(std::set<Chunk*> chnks, CNSocket* sock) {
    INITSTRUCT(sP_FE2CL_PC_EXIT, exitPlayer);

    // for chunks that need the player to be removed from
    for (Chunk* chunk : chnks) {

        // remove NPCs from view
        for (int32_t id : chunk->NPCs) {
            BaseNPC* npc = NPCManager::NPCs[id];
            switch (npc->npcClass) {
            case NPC_BUS:
                INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, exitBusData);
                exitBusData.eTT = 3;
                exitBusData.iT_ID = id;
                sock->sendPacket((void*)&exitBusData, P_FE2CL_TRANSPORTATION_EXIT, sizeof(sP_FE2CL_TRANSPORTATION_EXIT));
                break;
            case NPC_EGG:
                INITSTRUCT(sP_FE2CL_SHINY_EXIT, exitEggData);
                exitEggData.iShinyID = id;
                sock->sendPacket((void*)&exitEggData, P_FE2CL_SHINY_EXIT, sizeof(sP_FE2CL_SHINY_EXIT));
                break;
            default:
                INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
                exitData.iNPC_ID = id;
                sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
                break;
            }
        }

        // remove players from eachother's views
        for (CNSocket* otherSock : chunk->players) {
            if (sock == otherSock)
                continue; // that's us :P
            exitPlayer.iID = PlayerManager::getPlayer(sock)->iID;
            otherSock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
            exitPlayer.iID = PlayerManager::getPlayer(otherSock)->iID;
            sock->sendPacket((void*)&exitPlayer, P_FE2CL_PC_EXIT, sizeof(sP_FE2CL_PC_EXIT));
        }
    }

}

void ChunkManager::removeNPCFromChunks(std::set<Chunk*> chnks, int32_t id) {
    BaseNPC* npc = NPCManager::NPCs[id];

    switch (npc->npcClass) {
    case NPC_BUS:
        INITSTRUCT(sP_FE2CL_TRANSPORTATION_EXIT, exitBusData);
        exitBusData.eTT = 3;
        exitBusData.iT_ID = id;

        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitBusData, P_FE2CL_TRANSPORTATION_EXIT, sizeof(sP_FE2CL_TRANSPORTATION_EXIT));
            }
        }
        break;
    case NPC_EGG:
        INITSTRUCT(sP_FE2CL_SHINY_EXIT, exitEggData);
        exitEggData.iShinyID = id;

        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitEggData, P_FE2CL_SHINY_EXIT, sizeof(sP_FE2CL_SHINY_EXIT));
            }
        }
        break;
    default:
        // create struct
        INITSTRUCT(sP_FE2CL_NPC_EXIT, exitData);
        exitData.iNPC_ID = id;

        // remove it from the clients
        for (Chunk* chunk : chnks) {
            for (CNSocket* sock : chunk->players) {
                // send to socket
                sock->sendPacket((void*)&exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT));
            }
        }
        break;
    }
}

void ChunkManager::emptyChunk(ChunkPos chunkPos) {
    if (!chunkExists(chunkPos)) {
        std::cout << "[WARN] Tried to empty chunk that doesn't exist\n";
        return; // chunk doesn't exist, we don't need to do anything
    }

    Chunk* chunk = chunks[chunkPos];

    if (chunk->players.size() > 0) {
        std::cout << "[WARN] Tried to empty chunk that still had players\n";
        return; // chunk doesn't exist, we don't need to do anything
    }

    // unspawn all of the mobs/npcs
    std::set npcIDs(chunk->NPCs);
    for (uint32_t id : npcIDs) {
        // every call of this will check if the chunk is empty and delete it if so
        NPCManager::destroyNPC(id);
    }
}

bool ChunkManager::chunkExists(ChunkPos chunk) {
    return chunks.find(chunk) != chunks.end();
}

ChunkPos ChunkManager::chunkPosAt(int posX, int posY, uint64_t instanceID) {
    return std::make_tuple(posX / (settings::VIEWDISTANCE / 3), posY / (settings::VIEWDISTANCE / 3), instanceID);
}

std::set<Chunk*> ChunkManager::getViewableChunks(ChunkPos chunk) {
    std::set<Chunk*> chnks;

    int x, y;
    uint64_t inst;
    std::tie(x, y, inst) = chunk;

    // grabs surrounding chunks if they exist
    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            ChunkPos pos = std::make_tuple(x+i, y+z, inst);

            // if chunk exists, add it to the set
            if (chunkExists(pos))
                chnks.insert(chunks[pos]);
        }
    }

    return chnks;
}

/*
 * inefficient algorithm to get all chunks from a specific instance
 */
std::vector<ChunkPos> ChunkManager::getChunksInMap(uint64_t mapNum) {
    std::vector<ChunkPos> chnks;

    for (auto it = ChunkManager::chunks.begin(); it != ChunkManager::chunks.end(); it++) {
        if (std::get<2>(it->first) == mapNum) {
            chnks.push_back(it->first);
        }
    }

    return chnks;
}

bool ChunkManager::inPopulatedChunks(std::set<Chunk*>* chnks) {

    for (auto it = chnks->begin(); it != chnks->end(); it++) {
        if (!(*it)->players.empty())
            return true;
    }

    return false;
}

void ChunkManager::createInstance(uint64_t instanceID) {

    std::vector<ChunkPos> templateChunks = ChunkManager::getChunksInMap(MAPNUM(instanceID)); // base instance chunks
    if (ChunkManager::getChunksInMap(instanceID).size() == 0) { // only instantiate if the instance doesn't exist already
        std::cout << "Creating instance " << instanceID << std::endl;
        for (ChunkPos &coords : templateChunks) {
            for (int npcID : chunks[coords]->NPCs) {
                // make a copy of each NPC in the template chunks and put them in the new instance
                int newID = NPCManager::nextId++;
                BaseNPC* baseNPC = NPCManager::NPCs[npcID];
                if (baseNPC->npcClass == NPC_MOB) {
                    Mob* newMob = new Mob(baseNPC->appearanceData.iX, baseNPC->appearanceData.iY, baseNPC->appearanceData.iZ, baseNPC->appearanceData.iAngle,
                        instanceID, baseNPC->appearanceData.iNPCType, ((Mob*)baseNPC)->maxHealth, NPCManager::NPCData[baseNPC->appearanceData.iNPCType], newID);
                    NPCManager::NPCs[newID] = newMob;
                    MobManager::Mobs[newID] = newMob;
                } else {
                    BaseNPC* newNPC = new BaseNPC(baseNPC->appearanceData.iX, baseNPC->appearanceData.iY, baseNPC->appearanceData.iZ, baseNPC->appearanceData.iAngle,
                        instanceID, baseNPC->appearanceData.iNPCType, newID);
                    NPCManager::NPCs[newID] = newNPC;
                }
                NPCManager::updateNPCPosition(newID, baseNPC->appearanceData.iX, baseNPC->appearanceData.iY, baseNPC->appearanceData.iZ,
                    instanceID, baseNPC->appearanceData.iAngle);
            }
        }
    } else {
        std::cout << "Instance " << instanceID << " already exists" << std::endl;
    }
}

void ChunkManager::destroyInstance(uint64_t instanceID) {
    
    std::vector<ChunkPos> instanceChunks = ChunkManager::getChunksInMap(instanceID);
    std::cout << "Deleting instance " << instanceID << " (" << instanceChunks.size() << " chunks)" << std::endl;
    for (ChunkPos& coords : instanceChunks) {
        emptyChunk(coords);
    }
}

void ChunkManager::destroyInstanceIfEmpty(uint64_t instanceID) {
    if (PLAYERID(instanceID) == 0)
        return; // don't clean up overworld/IZ chunks

    std::vector<ChunkPos> sourceChunkCoords = getChunksInMap(instanceID);

    for (ChunkPos& coords : sourceChunkCoords) {
        Chunk* chunk = chunks[coords];

        if (chunk->players.size() > 0)
            return; // there are still players inside
    }

    destroyInstance(instanceID);
}
