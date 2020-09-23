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
    void removePlayer(std::pair<int, int> chunkPos, CNSocket* sock);
    void removeNPC(std::pair<int, int> chunkPos, int32_t id);
    bool checkChunk(std::pair<int, int> chunk);
    std::pair<int, int> grabChunk(int posX, int posY);
    std::vector<Chunk*> grabChunks(std::pair<int, int> chunkPos);
    std::vector<Chunk*> getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to);
    bool inPopulatedChunks(int posX, int posY);
}
