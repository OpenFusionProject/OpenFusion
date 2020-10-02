#include "TableData.hpp"
#include "NPCManager.hpp"
#include "TransportManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MissionManager.hpp"
#include "MobManager.hpp"
#include "ChunkManager.hpp"
#include "NanoManager.hpp"

#include "contrib/JSON.hpp"

#include <fstream>

void TableData::init() {
    int32_t nextId = 0;

    // load NPCs from NPC.json
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            BaseNPC *tmp = new BaseNPC(npc["x"], npc["y"], npc["z"], INSTANCE_OVERWORLD, npc["id"], nextId);

            NPCManager::NPCs[nextId] = tmp;
            NPCManager::updateNPCPosition(nextId, npc["x"], npc["y"], npc["z"]);
            nextId++;

            if (npc["id"] == 641 || npc["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["x"], npc["y"], ((int)npc["z"]) + RESURRECT_HEIGHT });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
    }

    loadPaths(&nextId); // load paths

    // load everything else from xdttable
    std::cout << "[INFO] Parsing xdt.json..." << std::endl;
    std::ifstream infile(settings::XDTJSON);
    nlohmann::json xdtData;

    // read file into json
    infile >> xdtData;

    // data we'll need for summoned mobs
    NPCManager::NPCData = xdtData["m_pNpcTable"]["m_pNpcData"];

    try {
        // load warps
        nlohmann::json warpData = xdtData["m_pInstanceTable"]["m_pWarpData"];

        for (nlohmann::json::iterator _warp = warpData.begin(); _warp != warpData.end(); _warp++) {
            auto warp = _warp.value();
            WarpLocation warpLoc = { warp["m_iToX"], warp["m_iToY"], warp["m_iToZ"] };
            int warpID = warp["m_iWarpNumber"];
            NPCManager::Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] Populated " << NPCManager::Warps.size() << " Warps" << std::endl;

        // load transport routes and locations
        nlohmann::json transRouteData = xdtData["m_pTransportationTable"]["m_pTransportationData"];
        nlohmann::json transLocData = xdtData["m_pTransportationTable"]["m_pTransportationWarpLocation"];

        for (nlohmann::json::iterator _tLoc = transLocData.begin(); _tLoc != transLocData.end(); _tLoc++) {
            auto tLoc = _tLoc.value();
            TransportLocation transLoc = { tLoc["m_iNPCID"], tLoc["m_iXpos"], tLoc["m_iYpos"], tLoc["m_iZpos"] };
            TransportManager::Locations[tLoc["m_iLocationID"]] = transLoc;
        }
        std::cout << "[INFO] Loaded " << TransportManager::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl;

        for (nlohmann::json::iterator _tRoute = transRouteData.begin(); _tRoute != transRouteData.end(); _tRoute++) {
            auto tRoute = _tRoute.value();
            TransportRoute transRoute = { tRoute["m_iMoveType"], tRoute["m_iStartLocation"], tRoute["m_iEndLocation"],
                tRoute["m_iCost"] , tRoute["m_iSpeed"], tRoute["m_iRouteNum"] };
            TransportManager::Routes[tRoute["m_iVehicleID"]] = transRoute;
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

        // load all item data. i'm sorry. it has to be done
        const char* setNames[12] = { "m_pBackItemTable", "m_pFaceItemTable", "m_pGlassItemTable", "m_pHatItemTable",
        "m_pHeadItemTable", "m_pPantsItemTable", "m_pShirtsItemTable", "m_pShoesItemTable", "m_pWeaponItemTable",
        "m_pVehicleItemTable", "m_pGeneralItemTable", "m_pChestItemTable" };
        nlohmann::json itemSet;
        for (int i = 0; i < 12; i++) {
            itemSet = xdtData[setNames[i]]["m_pItemData"];
            for (nlohmann::json::iterator _item = itemSet.begin(); _item != itemSet.end(); _item++) {
                auto item = _item.value();
                int typeOverride = getItemType(i); // used for special cases where iEquipLoc doesn't indicate item type
                ItemManager::ItemData[std::pair<int32_t, int32_t>(item["m_iItemNumber"], typeOverride != -1 ? typeOverride : (int)item["m_iEquipLoc"])]
                = { item["m_iTradeAble"] == 1, item["m_iSellAble"] == 1, item["m_iItemPrice"], item["m_iItemSellPrice"], item["m_iStackNumber"], i > 9 ? 0 : (int)item["m_iMinReqLev"], i > 9 ? 1 : (int)item["m_iRarity"], i > 9 ? 0 : (int)item["m_iPointRat"], i > 9 ? 0 : (int)item["m_iGroupRat"], i > 9 ? 0 : (int)item["m_iDefenseRat"] };
            }
        }

        std::cout << "[INFO] Loaded " << ItemManager::ItemData.size() << " items" << std::endl;

        // load player limits from m_pAvatarTable.m_pAvatarGrowData
        
        nlohmann::json growth = xdtData["m_pAvatarTable"]["m_pAvatarGrowData"];

        for (int i = 0; i < 37; i++) {
            MissionManager::AvatarGrowth[i] = growth[i];
        }

        // load vendor listings
        nlohmann::json listings = xdtData["m_pVendorTable"]["m_pItemData"];

        for (nlohmann::json::iterator _lst = listings.begin(); _lst != listings.end(); _lst++) {
            auto lst = _lst.value();
            VendorListing vListing = { lst["m_iSortNumber"], lst["m_iItemType"], lst["m_iitemID"] };
            ItemManager::VendorTables[lst["m_iNpcNumber"]].push_back(vListing);
        }

        std::cout << "[INFO] Loaded " << ItemManager::VendorTables.size() << " vendor tables" << std::endl;

        // load crocpot entries
        nlohmann::json crocs = xdtData["m_pCombiningTable"]["m_pCombiningData"];

        for (nlohmann::json::iterator croc = crocs.begin(); croc != crocs.end(); croc++) {
            CrocPotEntry crocEntry = { croc.value()["m_iStatConstant"], croc.value()["m_iLookConstant"], croc.value()["m_fLevelGapStandard"],
                croc.value()["m_fSameGrade"], croc.value()["m_fOneGrade"], croc.value()["m_fTwoGrade"], croc.value()["m_fThreeGrade"] };
            ItemManager::CrocPotTable[croc.value()["m_iLevelGap"]] = crocEntry;
        }

        std::cout << "[INFO] Loaded " << ItemManager::CrocPotTable.size() << " croc pot value sets" << std::endl;

        //load nano info
        nlohmann::json nanoInfo = xdtData["m_pNanoTable"]["m_pNanoData"];
        for (nlohmann::json::iterator _nano = nanoInfo.begin(); _nano != nanoInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoData nanoData;
            nanoData.style = nano["m_iStyle"];
            NanoManager::NanoTable[nano["m_iNanoNumber"]] = nanoData;
        }

        std::cout << "[INFO] Loaded " << NanoManager::NanoTable.size() << " nanos" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed xdt.json file! Reason:" << err.what() << std::endl;
    }

    // load temporary mob dump
    try {
        std::ifstream inFile(settings::MOBJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            auto td = NPCManager::NPCData[(int)npc["iNPCType"]];
            Mob *tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], INSTANCE_OVERWORLD, npc["iNPCType"], npc["iHP"], npc["iAngle"], td, nextId);

            NPCManager::NPCs[nextId] = tmp;
            MobManager::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            NPCManager::updateNPCPosition(nextId, npc["iX"], npc["iY"], npc["iZ"]);

            nextId++;
        }

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
    }

    NPCManager::nextId = nextId;
}

/*
 * Some item categories either don't possess iEquipLoc or use a different value for item type.
 */
int TableData::getItemType(int itemSet) {
    int overriden;
    switch (itemSet)
    {
    case 11: // Chest items don't have iEquipLoc and are type 9.
        overriden = 9;
        break;
    case 10: // General items don't have iEquipLoc and are type 7.
        overriden = 7;
        break;
    case 9: // Vehicles have iEquipLoc 8, but type 10.
        overriden = 10;
        break;
    default:
        overriden = -1;
    }
    return overriden;
}

/*
 * Load paths from paths JSON.
 */
void TableData::loadPaths(int* nextId) {
    try {
        std::ifstream inFile(settings::PATHJSON);
        nlohmann::json pathData;

        // read file into json
        inFile >> pathData;

        // skyway paths
        nlohmann::json pathDataSkyway = pathData["skyway"];
        for (nlohmann::json::iterator skywayPath = pathDataSkyway.begin(); skywayPath != pathDataSkyway.end(); skywayPath++) {
            constructPathSkyway(skywayPath);
        }
        std::cout << "[INFO] Loaded " << TransportManager::SkywayPaths.size() << " skyway paths" << std::endl;

        // slider circuit
        int sliders = 0;
        nlohmann::json pathDataSlider = pathData["slider"];
        for (nlohmann::json::iterator _sliderPoint = pathDataSlider.begin(); _sliderPoint != pathDataSlider.end(); _sliderPoint++) {
            auto sliderPoint = _sliderPoint.value();
            if (sliderPoint["stop"] && sliders % 2 == 0) { // check if this point in the circuit is a stop
                // spawn a slider
                BaseNPC* slider = new BaseNPC(sliderPoint["iX"], sliderPoint["iY"], sliderPoint["iZ"], 1, (*nextId)++, NPC_BUS);
                NPCManager::NPCs[slider->appearanceData.iNPC_ID] = slider;
                NPCManager::updateNPCPosition(slider->appearanceData.iNPC_ID, slider->appearanceData.iX, slider->appearanceData.iY, slider->appearanceData.iZ);
                // set slider path to a rotation of the circuit
                constructPathSlider(pathDataSlider, 0, slider->appearanceData.iNPC_ID);
                sliders++;
            }
        }

        // npc paths
        nlohmann::json pathDataNPC = pathData["npc"];
        for (nlohmann::json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            constructPathNPC(npcPath);
        }
        std::cout << "[INFO] Loaded " << TransportManager::NPCQueues.size() << " NPC paths" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed paths.json file! Reason:" << err.what() << std::endl;
    }
}

/*
 * Create a full and properly-paced path by interpolating between keyframes.
 */
void TableData::constructPathSkyway(nlohmann::json::iterator _pathData) {
    auto pathData = _pathData.value();
    // Interpolate
    nlohmann::json pathPoints = pathData["points"];
    std::queue<WarpLocation> points;
    nlohmann::json::iterator _point = pathPoints.begin();
    auto point = _point.value();
    WarpLocation last = { point["iX"] , point["iY"] , point["iZ"] }; // start pos
    // use some for loop trickery; start position should not be a point
    for (_point++; _point != pathPoints.end(); _point++) {
        point = _point.value();
        WarpLocation coords = { point["iX"] , point["iY"] , point["iZ"] };
        TransportManager::lerp(&points, last, coords, pathData["iMonkeySpeed"]);
        points.push(coords); // add keyframe to the queue
        last = coords; // update start pos
    }
    TransportManager::SkywayPaths[pathData["iRouteID"]] = points;
}

void TableData::constructPathSlider(nlohmann::json points, int rotations, int sliderID) {

    std::queue<WarpLocation> route;
    std::rotate(points.begin(), points.begin() + rotations, points.end()); // rotate points
    nlohmann::json::iterator _point = points.begin(); // iterator
    auto point = _point.value();
    WarpLocation from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
    int stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0; // arbitrary stop length
    for (_point++; _point != points.end(); _point++) { // loop through all point Bs
        point = _point.value();
        for (int i = 0; i < stopTime + 1; i++) // repeat point if it's a stop
            route.push(from); // add point A to the queue
        WarpLocation to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
        float curve = 1;
        if (stopTime > 0) { // point A is a stop
            curve = 2.0f;
        } else if (point["stop"]) { // point B is a stop
            curve = 0.35f;
        }
        TransportManager::lerp(&route, from, to, SLIDER_SPEED, curve); // lerp from A to B (arbitrary speed)
        from = to; // update point A
        stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0;
    }
    std::rotate(points.rbegin(), points.rbegin() + rotations, points.rend()); // undo rotation
    TransportManager::NPCQueues[sliderID] = route;
}

void TableData::constructPathNPC(nlohmann::json::iterator _pathData) {
    auto pathData = _pathData.value();
    // Interpolate
    nlohmann::json pathPoints = pathData["points"];
    std::queue<WarpLocation> points;
    nlohmann::json::iterator _point = pathPoints.begin();
    auto point = _point.value();
    WarpLocation from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
    int stopTime = point["stop"];
    for (_point++; _point != pathPoints.end(); _point++) { // loop through all point Bs
        point = _point.value();
        for(int i = 0; i < stopTime + 1; i++) // repeat point if it's a stop
            points.push(from); // add point A to the queue
        WarpLocation to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
        TransportManager::lerp(&points, from, to, pathData["iBaseSpeed"]); // lerp from A to B
        from = to; // update point A
        stopTime = point["stop"];
    }
    TransportManager::NPCQueues[pathData["iNPCID"]] = points;
}
