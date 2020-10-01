#pragma once

#include "CNStructs.hpp"
#include "ChunkManager.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;
    int instanceID;
    std::tuple<int, int, int> chunkPos;
    std::vector<Chunk*> currentChunks;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int iID, int type, int id) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = 0;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        instanceID = iID;

        chunkPos = std::make_tuple(0, 0, instanceID);
    };
    BaseNPC(int x, int y, int z, int iID, int type, int id, NPCClass classType) : BaseNPC(x, y, z, iID, type, id) {
        npcClass = classType;
    }
};
