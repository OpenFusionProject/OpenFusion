#pragma once

#include "core/Core.hpp"
#include "Entities.hpp"

#include <utility>
#include <set>
#include <map>
#include <tuple>
#include <algorithm>

class Chunk {
public:
    //std::set<CNSocket*> players;
    //std::set<int32_t> NPCs;
    std::set<EntityRef> entities;
    int nplayers = 0;
};

enum {
    INSTANCE_OVERWORLD, // default instance every player starts in
    INSTANCE_IZ, // these aren't actually used
    INSTANCE_UNIQUE // these aren't actually used
};

namespace Chunking {
    extern std::map<ChunkPos, Chunk*> chunks;

    void updateEntityChunk(const EntityRef& ref, ChunkPos from, ChunkPos to);

    void trackEntity(ChunkPos chunkPos, const EntityRef& ref);
    void untrackEntity(ChunkPos chunkPos, const EntityRef& ref);

    void addEntityToChunks(std::set<Chunk*> chnks, const EntityRef& ref);
    void removeEntityFromChunks(std::set<Chunk*> chnks, const EntityRef& ref);

    bool chunkExists(ChunkPos chunk);
    ChunkPos chunkPosAt(int posX, int posY, uint64_t instanceID);
    std::set<Chunk*> getViewableChunks(ChunkPos chunkPos);
    std::vector<ChunkPos> getChunksInMap(uint64_t mapNum);

    bool inPopulatedChunks(std::set<Chunk*>* chnks);
    void createInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}
