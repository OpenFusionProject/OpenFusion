#include "TableData.hpp"
#include "NPCManager.hpp"
#include "TransportManager.hpp"
#include "ItemManager.hpp"
#include "settings.hpp"
#include "MissionManager.hpp"
#include "MobManager.hpp"
#include "ChunkManager.hpp"

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

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            BaseNPC *tmp = new BaseNPC(npc["x"], npc["y"], npc["z"], npc["id"]);

            // Temporary fix, IDs will be pulled from json later
            tmp->appearanceData.iNPC_ID = i;

            NPCManager::NPCs[i] = tmp;
            ChunkManager::addNPC(npc["x"], npc["y"], i);
            i++;

            if (npc["id"] == 641 || npc["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["x"], npc["y"], ((int)npc["z"]) + RESURRECT_HEIGHT });
        }

    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
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
        std::cout << "[INFO] Loaded " << TransportManager::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl; // TODO: Skyway operates differently

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
                = { item["m_iTradeAble"] == 1, item["m_iSellAble"] == 1, item["m_iItemPrice"], item["m_iItemSellPrice"], item["m_iStackNumber"], i > 9 ? 0 : (int)item["m_iMinReqLev"],
                i > 9 ? 1 : (int)item["m_iRarity"] };
            }
        }

        std::cout << "[INFO] Loaded " << ItemManager::ItemData.size() << " items" << std::endl;

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

        nlohmann::json npcTableData = xdtData["m_pNpcTable"]["m_pNpcData"];

        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            auto td = npcTableData[(int)npc["iNPCType"]];
            Mob *tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iNPCType"], npc["iHP"], npc["iAngle"], td["m_iRegenTime"]);

            // Temporary fix, IDs will be pulled from json later
            tmp->appearanceData.iNPC_ID = i;

            NPCManager::NPCs[i] = tmp;
            MobManager::Mobs[i] = (Mob*)NPCManager::NPCs[i];
            ChunkManager::addNPC(npc["iX"], npc["iY"], i);

            i++;
        }

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[WARN] Malformed mobs.json file! Reason:" << err.what() << std::endl;
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
    for (auto& pair : NPCManager::NPCs)
        delete pair.second;
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
