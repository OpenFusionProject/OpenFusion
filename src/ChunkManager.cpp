#include "ChunkManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"

std::map<std::pair<int, int>, Chunk*> ChunkManager::chunks;

void ChunkManager::init() {} // stubbed
void ChunkManager::cleanup() {
    // cleans up all the allocated chunks
    for (auto& pair : chunks) {
        delete pair.second;
    }
}

void ChunkManager::addNPC(int posX, int posY, int32_t id) {
    std::pair<int, int> pos = grabChunk(posX, posY);

    // make chunk if it doesn't exist!
    if (chunks.find(pos) == chunks.end()) {
        chunks[pos] = new Chunk();
        chunks[pos]->players = std::set<CNSocket*>();
        chunks[pos]->NPCs = std::set<int32_t>();
    }

    Chunk* chunk = chunks[pos];

    chunk->NPCs.insert(id);
    
    NPCManager::addNPC(grabChunks(pos.first, pos.second), id);
}

void ChunkManager::addPlayer(int posX, int posY, CNSocket* sock) {
    std::pair<int, int> pos = grabChunk(posX, posY);

    // make chunk if it doesn't exist!
    if (chunks.find(pos) == chunks.end()) {
        chunks[pos] = new Chunk();
        chunks[pos]->players = std::set<CNSocket*>();
        chunks[pos]->NPCs = std::set<int32_t>();
    }

    Chunk* chunk = chunks[pos];

    chunk->players.insert(sock);
}

std::pair<int, int> ChunkManager::grabChunk(int posX, int posY) {
    return std::make_pair<int, int>(posX / (settings::PLAYERDISTANCE / 3), posY / (settings::PLAYERDISTANCE / 3));
}

std::vector<Chunk*> ChunkManager::grabChunks(int chunkX, int chunkY) {
    std::vector<Chunk*> delta;
    delta.reserve(9);

    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            std::pair<int, int> pos(chunkX+i, chunkY+z);
            
            // if chunk exists, add it to the delta
            if (chunks.find(pos) != chunks.end()) {
                delta.push_back(chunks[pos]);
            }
        }
    }

    return delta;
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