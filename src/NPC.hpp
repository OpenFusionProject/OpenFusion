#pragma once

#include "CNStructs.hpp"
#include "ChunkManager.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;
    std::tuple<int, int, int> chunkPos;
    std::vector<Chunk*> currentChunks;
    int imapNum = 0;
    int jsonRef = 0;
    BaseNPC() {};
    BaseNPC(int x, int y, int z, int type, int id, int mapNum, int jsonInd) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = 0;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;
        imapNum = mapNum;
        jsonRef = jsonInd;
        chunkPos = std::tuple<int, int, int>(0, 0, 0);
    };
    BaseNPC(int x, int y, int z, int type, int id, int mapNum) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = 0;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;
        imapNum = mapNum;
        chunkPos = std::tuple<int, int, int>(0, 0, 0);
    };
    BaseNPC(int x, int y, int z, int type, int id, int mapNum,int jsonInd, NPCClass classType) : BaseNPC(x, y, z, type, id , mapNum, jsonInd) {
        npcClass = classType;
    }
};
