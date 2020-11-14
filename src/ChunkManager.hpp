#pragma once

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

#include <utility>
#include <vector>
#include <set>
#include <map>
#include <tuple>

class Chunk {
public:
    std::set<CNSocket*> players;
    std::set<int32_t> NPCs;
};

enum {
    INSTANCE_OVERWORLD, // default instance every player starts in
    INSTANCE_IZ, // these aren't actually used
    INSTANCE_UNIQUE // these aren't actually used
};

namespace ChunkManager {
    void init();
    void cleanup();

    extern std::map<CHUNKPOS, Chunk*> chunks;

    void newChunk(CHUNKPOS pos);
    void populateNewChunk(Chunk* chunk, CHUNKPOS pos);
    void addNPC(int posX, int posY, uint64_t instanceID, int32_t id);
    void addPlayer(int posX, int posY, uint64_t instanceID, CNSocket* sock);
    bool removePlayer(CHUNKPOS chunkPos, CNSocket* sock);
    bool removeNPC(CHUNKPOS chunkPos, int32_t id);
    bool checkChunk(CHUNKPOS chunk);
    void destroyChunk(CHUNKPOS chunkPos);
    CHUNKPOS grabChunk(int posX, int posY, uint64_t instanceID);
    std::vector<Chunk*> grabChunks(CHUNKPOS chunkPos);
    std::vector<Chunk*> getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to);
    std::vector<CHUNKPOS> getChunksInMap(uint64_t mapNum);
    bool inPopulatedChunks(int posX, int posY, uint64_t instanceID);

    void createInstance(uint64_t);
    void destroyInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}
