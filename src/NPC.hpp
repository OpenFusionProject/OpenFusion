#pragma once

#include "CNStructs.hpp"
#include "ChunkManager.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;
    uint64_t instanceID;
    ChunkPos chunkPos;
    std::set<Chunk*>* viewableChunks;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = angle;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;
        appearanceData.iNPC_ID = id;

        instanceID = iID;

        chunkPos = std::make_tuple(0, 0, 0);
        viewableChunks = new std::set<Chunk*>();
    };
    BaseNPC(int x, int y, int z, int angle, uint64_t iID, int type, int id, NPCClass classType) : BaseNPC(x, y, z, angle, iID, type, id) {
        npcClass = classType;
    }
};
