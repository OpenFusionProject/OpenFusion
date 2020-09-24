#pragma once

#include "CNStructs.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;
    NPCClass npcClass;

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
    };
    BaseNPC(int x, int y, int z, int type, int id, NPCClass classType) : BaseNPC(x, y, z, type, id) {
        npcClass = classType;
    }
};
