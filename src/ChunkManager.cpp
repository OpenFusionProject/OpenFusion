#include "ChunkManager.hpp"
#include "PlayerManager.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"

std::map<std::pair<int, int>, Chunk*> ChunkManager::chunks;

void ChunkManager::init() {} // stubbed

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

void ChunkManager::removePlayer(std::pair<int, int> chunkPos, CNSocket* sock) {
    if (!checkChunk(chunkPos))
        return; // do nothing if chunk doesn't even exist
    
    Chunk* chunk = chunks[chunkPos];

    chunk->players.erase(sock); // gone

    // TODO: if players and NPCs are empty, free chunk and remove it from surrounding views
}

void ChunkManager::removeNPC(std::pair<int, int> chunkPos, int32_t id) {
    if (!checkChunk(chunkPos))
        return; // do nothing if chunk doesn't even exist

    Chunk* chunk = chunks[chunkPos];

    chunk->NPCs.erase(id); // gone

    // TODO: if players and NPCs are empty, free chunk and remove it from surrounding views
}

bool ChunkManager::checkChunk(std::pair<int, int> chunk) {
    return chunks.find(chunk) != chunks.end();
}

std::pair<int, int> ChunkManager::grabChunk(int posX, int posY) {
    return std::make_pair<int, int>(posX / (settings::CHUNKSIZE / 3), posY / (settings::CHUNKSIZE / 3));
}

std::vector<Chunk*> ChunkManager::grabChunks(std::pair<int, int> chunk) {
    std::vector<Chunk*> chnks;
    chnks.reserve(9);

    // grabs surrounding chunks if they exist
    for (int i = -1; i < 2; i++) {
        for (int z = -1; z < 2; z++) {
            std::pair<int, int> pos(chunk.first+i, chunk.second+z);
            
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

bool ChunkManager::inPopulatedChunks(int posX, int posY) {
    auto chunk = ChunkManager::grabChunk(posX, posY);
    auto nearbyChunks = ChunkManager::grabChunks(chunk);

    for (Chunk *c: nearbyChunks) {
        if (!c->players.empty())
            return true;
    }

    return false;
}
