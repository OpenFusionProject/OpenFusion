#include "TableData.hpp"
#include "NPCManager.hpp"
#include "TransportManager.hpp"
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
        std::ifstream inFile(settings::MOBJSON);
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

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
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

        std::cout << "[INFO] Populated " << NPCManager::Warps.size() << " Warps" << std::endl;

        // load transport routes and locations
        nlohmann::json transRouteData = xdtData["m_pTransportationTable"]["m_pTransportationData"];
        nlohmann::json transLocData = xdtData["m_pTransportationTable"]["m_pTransportationWarpLocation"];

        for (nlohmann::json::iterator tLoc = transLocData.begin(); tLoc != transLocData.end(); tLoc++) {
            TransportLocation transLoc = { tLoc.value()["m_iNPCID"], tLoc.value()["m_iXpos"], tLoc.value()["m_iYpos"], tLoc.value()["m_iZpos"] };
            TransportManager::Locations[tLoc.value()["m_iLocationID"]] = transLoc;
        }
        std::cout << "[INFO] Loaded " << TransportManager::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl; // TODO: Skyway operates differently

        for (nlohmann::json::iterator tRoute = transRouteData.begin(); tRoute != transRouteData.end(); tRoute++) {
            TransportRoute transRoute = { tRoute.value()["m_iMoveType"], tRoute.value()["m_iStartLocation"], tRoute.value()["m_iEndLocation"],
                tRoute.value()["m_iCost"] , tRoute.value()["m_iSpeed"], tRoute.value()["m_iRouteNum"] };
            TransportManager::Routes[tRoute.value()["m_iVehicleID"]] = transRoute;
        }
        std::cout << "[INFO] Loaded " << TransportManager::Routes.size() << " transportation routes" << std::endl;

        // load mission-related data
        nlohmann::json tasks = xdtData["m_pMissionTable"]["m_pMissionData"];

        for (auto _task = tasks.begin(); _task != tasks.end(); _task++) {
            auto task = _task.value();

            // rewards
            if (task["m_iSUReward"] != 0) {
                auto _rew = xdtData["m_pMissionTable"]["m_pRewardData"][(int)task["m_iSUReward"]];
                Reward *rew = new Reward(_rew["m_iMissionRewardID"], _rew["m_iMissionRewarItemType"],
                        _rew["m_iMissionRewardItemID"], _rew["m_iCash"], _rew["m_iFusionMatter"]);

                MissionManager::Rewards[task["m_iHTaskID"]] = rew;
            }

            // everything else lol. see TaskData comment.
            MissionManager::Tasks[task["m_iHTaskID"]] = new TaskData(task);
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
    for (auto& pair : MissionManager::Tasks)
        delete pair.second;
}
