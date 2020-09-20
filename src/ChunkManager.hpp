#pragma once

#include "CNProtocol.hpp"

#include <utility>
#include <vector>
#include <set>
#include <map>

class Chunk {
public:
    std::set<CNSocket*> players;
    std::set<int32_t> NPCs;
};

namespace ChunkManager {
    void init();
    void cleanup();

    extern std::map<std::pair<int, int>, Chunk*> chunks;

    void addNPC(int posX, int posY, int32_t id);
    void addPlayer(int posX, int posY, CNSocket* sock);
    std::pair<int, int> grabChunk(int posX, int posY);
    std::vector<Chunk*> grabChunks(int chunkX, int chunkY);
    std::vector<Chunk*> getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to);
}
