#pragma once

#include "CNStructs.hpp"

class BaseNPC {
public:
    sNPCAppearanceData appearanceData;

    BaseNPC() {};
    BaseNPC(int x, int y, int z, int type) {
        appearanceData.iX = x;
        appearanceData.iY = y;
        appearanceData.iZ = z;
        appearanceData.iNPCType = type;
        appearanceData.iHP = 400;
        appearanceData.iAngle = 0;
        appearanceData.iConditionBitFlag = 0;
        appearanceData.iBarkerType = 0;

        // hopefully no collisions happen :eyes:
        appearanceData.iNPC_ID = (int32_t)rand();
    };
};
