#include "TableData.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"
#include "contrib/JSON.hpp"

#include <fstream>

void TabledataManager::init() {
    int i = 0;

    // load NPCs from NPC.json
    try {

        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["x"], npc.value()["y"], npc.value()["z"], npc.value()["id"]);
            
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;
            
            NPCManager::NPCs[tmp.appearanceData.iNPC_ID] = tmp;

            if (npc.value()["id"] == 641 || npc.value()["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc.value()["x"], npc.value()["y"], ((int)npc.value()["z"]) + RESURRECT_HEIGHT });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    // load temporary mob dump
    try {
        std::ifstream inFile(settings::MOBJSON); // not in settings, since it's temp
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator npc = npcData.begin(); npc != npcData.end(); npc++) {
            BaseNPC tmp(npc.value()["iX"], npc.value()["iY"], npc.value()["iZ"], npc.value()["iNPCType"],
                npc.value()["iHP"], npc.value()["iConditionBitFlag"], npc.value()["iAngle"], npc.value()["iBarkerType"]);
            
            // Temporary fix, IDs will be pulled from json later
            tmp.appearanceData.iNPC_ID = i;
            i++;
            
            NPCManager::NPCs[tmp.appearanceData.iNPC_ID] = tmp;
        }

        std::cout << "[INFO] populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
    }


    // load everything else from xdttable
    try {
        std::ifstream infile(settings::XDTJSON);
        nlohmann::json xdtData;

        // read file into json
        infile >> xdtData;

        // load warps from m_pInstanceTable.m_pWarpData
        nlohmann::json warpData = xdtData["m_pInstanceTable"]["m_pWarpData"];

        for (nlohmann::json::iterator warp = warpData.begin(); warp != warpData.end(); warp++) {
            WarpLocation warpLoc = { warp.value()["m_iToX"], warp.value()["m_iToY"], warp.value()["m_iToZ"] };
            int warpID = warp.value()["m_iWarpNumber"];
            NPCManager::Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] populated " << NPCManager::Warps.size() << " Warps" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed xdt.json file! Reason:" << err.what() << std::endl;
    }

}