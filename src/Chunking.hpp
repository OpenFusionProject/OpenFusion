#pragma once

#include "Entities.hpp"

#include <set>
#include <map>
#include <vector>

struct EntityRef;

class Chunk {
public:
    std::set<EntityRef> entities;
    int nplayers = 0;
};

// to help the readability of ChunkPos
typedef std::tuple<int, int, uint64_t> _ChunkPos;

class ChunkPos : public _ChunkPos {
public:
    ChunkPos() : _ChunkPos(0, 0, (uint64_t) -1) {}
    ChunkPos(int x, int y, uint64_t inst) : _ChunkPos(x, y, inst) {}
};

enum {
    INSTANCE_OVERWORLD, // default instance every player starts in
    INSTANCE_IZ, // these aren't actually used
    INSTANCE_UNIQUE // these aren't actually used
};

namespace Chunking {
    extern std::map<ChunkPos, Chunk*> chunks;

    extern const ChunkPos INVALID_CHUNK;

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
