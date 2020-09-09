#include "TableData.hpp"
#include "NPCManager.hpp"
#include "settings.hpp"
#include "MissionManager.hpp"

#include "contrib/JSON.hpp"

#include <fstream>

void TableData::init() {
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
    std::cout << "[INFO] Parsing xdt.json..." << std::endl;
    std::ifstream infile(settings::XDTJSON);
    nlohmann::json xdtData;

    // read file into json
    infile >> xdtData;

    try {
        // load warps
        nlohmann::json warpData = xdtData["m_pInstanceTable"]["m_pWarpData"];

        for (nlohmann::json::iterator warp = warpData.begin(); warp != warpData.end(); warp++) {
            WarpLocation warpLoc = { warp.value()["m_iToX"], warp.value()["m_iToY"], warp.value()["m_iToZ"] };
            int warpID = warp.value()["m_iWarpNumber"];
            NPCManager::Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] populated " << NPCManager::Warps.size() << " Warps" << std::endl;

        // missions
        nlohmann::json tasks = xdtData["m_pMissionTable"]["m_pMissionData"];

        for (auto _task = tasks.begin(); _task != tasks.end(); _task++) {
            auto task = _task.value();

            // rewards
            if (task["m_iSUReward"] != 0) {
                auto tmp = xdtData["m_pMissionTable"]["m_pRewardData"][(int)task["m_iSUReward"]];
                Reward *rew = new Reward(tmp["m_iMissionRewardID"], tmp["m_iMissionRewarItemType"],
                        tmp["m_iMissionRewardItemID"], tmp["m_iCash"], tmp["m_iFusionMatter"]);

                MissionManager::Rewards[task["m_iHTaskID"]] = rew;
            }

            // quest items obtained after completing a certain task
            // (distinct from quest items dropped from mobs)
            if (task["m_iSUItem"][0] != 0) {
                MissionManager::SUItems[task["m_iHTaskID"]] = new SUItem(task["m_iSUItem"]);
            }

            // quest item mob drops
            if (task["m_iCSUItemID"][0] != 0) {
                MissionManager::QuestDropSets[task["m_iHTaskID"]] = new QuestDropSet(task["m_iCSUEnemyID"], task["m_iCSUItemID"]);
                // TODO: timeouts, drop rates, etc.
                //       not sure if we need to keep track of NumNeeded/NumToKill server-side.
            }
        }

        std::cout << "[INFO] Loaded mission-related data" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed xdt.json file! Reason:" << err.what() << std::endl;
    }
}

void TableData::cleanup() {
    /*
     * This is just to shut the address sanitizer up. Dynamically allocated data
     * doesn't need to be cleaned up if it's supposed to last the program's full runtime.
     */
    for (auto& pair : MissionManager::Rewards)
        delete pair.second;
    for (auto& pair : MissionManager::SUItems)
        delete pair.second;
    for (auto& pair : MissionManager::QuestDropSets)
        delete pair.second;
}
