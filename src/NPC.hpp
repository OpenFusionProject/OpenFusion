#pragma once

#include "CNStructs.hpp"
#include "ChunkManager.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;
    std::pair<int, int> chunkPos;
    std::vector<Chunk*> currentChunks;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int type, int id) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = 0;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        chunkPos = std::pair<int, int>(0, 0);
    };
    BaseNPC(int x, int y, int z, int type, int id, NPCClass classType) : BaseNPC(x, y, z, type, id) {
        npcClass = classType;
    }
};
