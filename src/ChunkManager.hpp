#pragma once

#include "CNProtocol.hpp"

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

    extern std::map<std::tuple<int, int, uint64_t>, Chunk*> chunks;

    void newChunk(std::tuple<int, int, uint64_t> pos);
    void addNPC(int posX, int posY, uint64_t instanceID, int32_t id);
    void addPlayer(int posX, int posY, uint64_t instanceID, CNSocket* sock);
    bool removePlayer(std::tuple<int, int, uint64_t> chunkPos, CNSocket* sock);
    bool removeNPC(std::tuple<int, int, uint64_t> chunkPos, int32_t id);
    bool checkChunk(std::tuple<int, int, uint64_t> chunk);
    void destroyChunk(std::tuple<int, int, uint64_t> chunkPos);
    std::tuple<int, int, uint64_t> grabChunk(int posX, int posY, uint64_t instanceID);
    std::vector<Chunk*> grabChunks(std::tuple<int, int, uint64_t> chunkPos);
    std::vector<Chunk*> getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to);
    std::vector<std::tuple<int, int, uint64_t>> getChunksInMap(uint64_t mapNum);
    bool inPopulatedChunks(int posX, int posY, uint64_t instanceID);

    void createInstance(uint64_t);
    void destroyInstance(uint64_t);
    void destroyInstanceIfEmpty(uint64_t);
}
