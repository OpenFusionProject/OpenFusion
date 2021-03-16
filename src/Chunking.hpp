#pragma once

#include "CNProtocol.hpp"
#include "CNStructs.hpp"

#include <utility>
#include <set>
#include <map>
#include <tuple>
#include <algorithm>

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

namespace Chunking {
    extern std::map<ChunkPos, Chunk*> chunks;

    void updatePlayerChunk(CNSocket* sock, ChunkPos from, ChunkPos to);
    void updateNPCChunk(int32_t id, ChunkPos from, ChunkPos to);

    void trackPlayer(ChunkPos chunkPos, CNSocket* sock);
    void trackNPC(ChunkPos chunkPos, int32_t id);
    void untrackPlayer(ChunkPos chunkPos, CNSocket* sock);
    void untrackNPC(ChunkPos chunkPos, int32_t id);

    void addPlayerToChunks(std::set<Chunk*> chnks, CNSocket* sock);
    void addNPCToChunks(std::set<Chunk*> chnks, int32_t id);
    void removePlayerFromChunks(std::set<Chunk*> chnks, CNSocket* sock);
    void removeNPCFromChunks(std::set<Chunk*> chnks, int32_t id);

    bool chunkExists(ChunkPos chunk);
    ChunkPos chunkPosAt(int posX, int posY, uint64_t instanceID);
    std::set<Chunk*> getViewableChunks(ChunkPos chunkPos);

    bool inPopulatedChunks(std::set<Chunk*>* chnks);
    void createInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}
