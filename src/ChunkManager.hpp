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

    extern std::map<std::tuple<int, int, int>, Chunk*> chunks;

    void addNPC(int posX, int posY, int mapNum, int32_t id);
    void addPlayer(int posX, int posY, int mapNum, CNSocket* sock);
    void removePlayer(std::tuple<int, int, int> chunkPos, CNSocket* sock);
    void removeNPC(std::tuple<int, int, int> chunkPos, int32_t id);
    bool checkChunk(std::tuple<int, int, int> chunk);
    std::tuple<int, int, int> grabChunk(int posX, int posY, int mapNum);
    std::vector<Chunk*> grabChunks(std::tuple<int, int, int> chunkPos);
    std::vector<Chunk*> getDeltaChunks(std::vector<Chunk*> from, std::vector<Chunk*> to);
    bool inPopulatedChunks(int posX, int posY, int mapNum);
}
