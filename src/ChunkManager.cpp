#include "ChunkManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"
#include "MobManager.hpp"

std::map<ChunkPos, Chunk*> ChunkManager::chunks;

void ChunkManager::init() {} // stubbed

void ChunkManager::newChunk(ChunkPos pos) {
    Chunk *chunk = new Chunk();

    chunk->players = std::set<CNSocket*>();
    chunk->NPCs = std::set<int32_t>();

    chunks[pos] = chunk;
}

void ChunkManager::populateNewChunk(Chunk* chunk, ChunkPos pos) {// add the new chunk to every player and mob that's near it
    for (Chunk *c : grabChunks(pos)) {
        if (c == chunk)
            continue;

        for (CNSocket *s : c->players)
            PlayerManager::players[s].currentChunks.push_back(chunk);

        for (int32_t id : c->NPCs)
            NPCManager::NPCs[id]->currentChunks.push_back(chunk);
    }
}

void ChunkManager::addNPC(int posX, int posY, uint64_t instanceID, int32_t id) {
    ChunkPos pos = grabChunk(posX, posY, instanceID);

    bool newChunkUsed = false;

    // make chunk if it doesn't exist!
    if (chunks.find(pos) == chunks.end()) {
        newChunk(pos);
        newChunkUsed = true;
    }

    Chunk* chunk = chunks[pos];

    if (newChunkUsed)
        NPCManager::NPCs[id]->currentChunks.push_back(chunk);

    chunk->NPCs.insert(id);

    // we must update other players after the NPC is added to chunk
    if (newChunkUsed)
        populateNewChunk(chunk, pos);
}

void ChunkManager::addPlayer(int posX, int posY, uint64_t instanceID, CNSocket* sock) {
    ChunkPos pos = grabChunk(posX, posY, instanceID);

    bool newChunkUsed = false;

    // make chunk if it doesn't exist!
    if (chunks.find(pos) == chunks.end()) {
        newChunk(pos);
        newChunkUsed = true;
    }

    Chunk* chunk = chunks[pos];

    if (newChunkUsed)
        PlayerManager::players[sock].currentChunks.push_back(chunk);

    chunk->players.insert(sock);

    // we must update other players after this player is added to chunk
    if (newChunkUsed)
        populateNewChunk(chunk, pos);
}

bool ChunkManager::removePlayer(ChunkPos chunkPos, CNSocket* sock) {
    if (!checkChunk(chunkPos))
        return false; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->players.erase(sock); // gone

    // if players and NPCs are empty, free chunk and remove it from surrounding views
    if (chunk->NPCs.size() == 0 && chunk->players.size() == 0) {
        destroyChunk(chunkPos);

        // the chunk we left was destroyed
        return true;
    }

    // the chunk we left was not destroyed
    return false;
}

bool ChunkManager::removeNPC(ChunkPos chunkPos, int32_t id) {
    if (!checkChunk(chunkPos))
        return false; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->NPCs.erase(id); // gone

    // if players and NPCs are empty, free chunk and remove it from surrounding views
    if (chunk->NPCs.size() == 0 && chunk->players.size() == 0) {
        destroyChunk(chunkPos);

        // the chunk we left was destroyed
        return true;
    }

    // the chunk we left was not destroyed
    return false;
}

void ChunkManager::destroyChunk(ChunkPos chunkPos) {
    if (!checkChunk(chunkPos))
        return; // chunk doesn't exist, we don't need to do anything

    Chunk* chunk = chunks[chunkPos];

    // unspawn all of the mobs/npcs
    std::set npcIDs(chunk->NPCs);
    for (uint32_t id : npcIDs) {
        NPCManager::destroyNPC(id);
    }

    // we also need to remove it from all NPCs/Players views
    for (Chunk* otherChunk : grabChunks(chunkPos)) {
        if (otherChunk == chunk)
            continue;

        // remove from NPCs
        for (uint32_t id : otherChunk->NPCs) {
            if (std::find(NPCManager::NPCs[id]->currentChunks.begin(), NPCManager::NPCs[id]->currentChunks.end(), chunk) != NPCManager::NPCs[id]->currentChunks.end()) {
                NPCManager::NPCs[id]->currentChunks.erase(std::remove(NPCManager::NPCs[id]->currentChunks.begin(), NPCManager::NPCs[id]->currentChunks.end(), chunk), NPCManager::NPCs[id]->currentChunks.end());
            }
        }

        // remove from players
        for (CNSocket* sock : otherChunk->players) {
            PlayerView* plyr = &PlayerManager::players[sock];
            if (std::find(plyr->currentChunks.begin(), plyr->currentChunks.end(), chunk) != plyr->currentChunks.end()) {
                plyr->currentChunks.erase(std::remove(plyr->currentChunks.begin(), plyr->currentChunks.end(), chunk), plyr->currentChunks.end());
            }
        }
    }


    assert(chunk->players.size() == 0);

    // remove from the map
    chunks.erase(chunkPos);

    delete chunk;
}

bool ChunkManager::checkChunk(ChunkPos chunk) {
    return chunks.find(chunk) != chunks.end();
}

ChunkPos ChunkManager::grabChunk(int posX, int posY, uint64_t instanceID) {
    return std::make_tuple(posX / (settings::VIEWDISTANCE / 3), posY / (settings::VIEWDISTANCE / 3), instanceID);
}

std::vector<Chunk*> ChunkManager::grabChunks(ChunkPos chunk) {
    std::vector<Chunk*> chnks;
    chnks.reserve(9);

    int x, y;
    uint64_t inst;
    std::tie(x, y, inst) = chunk;

    // grabs surrounding chunks if they exist
    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            ChunkPos pos = std::make_tuple(x+i, y+z, inst);

            // if chunk exists, add it to the vector
            if (checkChunk(pos))
                chnks.push_back(chunks[pos]);
        }
    }

    return chnks;
}

// returns the chunks that aren't shared (only from from)
std::vector<Chunk*> ChunkManager::getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to) {
    std::vector<Chunk*> delta;

    for (Chunk* i : from) {
        bool found = false;

        // search for it in the other array
        for (Chunk* z : to) {
            if (i == z) {
                found = true;
                break;
            }
        }

        // add it to the vector if we didn't find it!
        if (!found)
            delta.push_back(i);
    }

    return delta;
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

bool ChunkManager::inPopulatedChunks(int posX, int posY, uint64_t instanceID) {
    auto chunk = ChunkManager::grabChunk(posX, posY, instanceID);
    auto nearbyChunks = ChunkManager::grabChunks(chunk);

    for (Chunk *c: nearbyChunks) {
        if (!c->players.empty())
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
                NPCManager::updateNPCInstance(newID, instanceID); // make sure the npc state gets updated
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
        destroyChunk(coords);
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
