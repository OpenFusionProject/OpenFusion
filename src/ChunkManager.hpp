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

namespace ChunkManager {
    void init();
    void cleanup();

    extern std::map<ChunkPos, Chunk*> chunks;

    void newChunk(ChunkPos pos);
    void deleteChunk(ChunkPos pos);

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
    void emptyChunk(ChunkPos chunkPos);
    ChunkPos chunkPosAt(int posX, int posY, uint64_t instanceID);
    std::set<Chunk*> getViewableChunks(ChunkPos chunkPos);

    std::vector<ChunkPos> getChunksInMap(uint64_t mapNum);
    bool inPopulatedChunks(std::set<Chunk*>* chnks);
    void createInstance(uint64_t);
    void destroyInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}
