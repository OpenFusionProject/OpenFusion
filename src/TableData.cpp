#include "TableData.hpp"
#include "NPCManager.hpp"
#include "Transport.hpp"
#include "Items.hpp"
#include "settings.hpp"
#include "Missions.hpp"
#include "Chunking.hpp"
#include "Nanos.hpp"
#include "Racing.hpp"
#include "Vendors.hpp"
#include "Abilities.hpp"
#include "Eggs.hpp"

#include "JSON.hpp"

#include <fstream>
#include <sstream>
#include <cmath>

using namespace TableData;

std::map<int32_t, std::vector<Vec3>> TableData::RunningSkywayRoutes;
std::map<int32_t, int> TableData::RunningNPCRotations;
std::map<int32_t, int> TableData::RunningNPCMapNumbers;
std::unordered_map<int32_t, std::pair<BaseNPC*, std::vector<BaseNPC*>>> TableData::RunningNPCPaths;
std::vector<NPCPath> TableData::FinishedNPCPaths;
std::map<int32_t, BaseNPC*> TableData::RunningMobs;
std::map<int32_t, BaseNPC*> TableData::RunningGroups;
std::map<int32_t, BaseNPC*> TableData::RunningEggs;

class TableException : public std::exception {
public:
    std::string msg;

    TableException(std::string m) : std::exception() { msg = m; }

    const char *what() const throw() { return msg.c_str(); }
};

/*
 * Create a full and properly-paced path by interpolating between keyframes.
 */
static void constructPathSkyway(json& pathData) {
    // Interpolate
    json pathPoints = pathData["aPoints"];
    std::queue<Vec3> points;
    json::iterator _point = pathPoints.begin();
    auto point = _point.value();
    Vec3 last = { point["iX"] , point["iY"] , point["iZ"] }; // start pos
    // use some for loop trickery; start position should not be a point
    for (_point++; _point != pathPoints.end(); _point++) {
        point = _point.value();
        Vec3 coords = { point["iX"] , point["iY"] , point["iZ"] };
        Transport::lerp(&points, last, coords, pathData["iMonkeySpeed"]);
        points.push(coords); // add keyframe to the queue
        last = coords; // update start pos
    }
    Transport::SkywayPaths[pathData["iRouteID"]] = points;
}

/*
 * Load all relevant data from the XDT into memory
 * This should be called first, before any of the other load functions
 */
static void loadXDT(json& xdtData) {
    // data we'll need for summoned mobs
    NPCManager::NPCData = xdtData["m_pNpcTable"]["m_pNpcData"];

    try {
        // load warps
        json warpData = xdtData["m_pInstanceTable"]["m_pWarpData"];

        for (json::iterator _warp = warpData.begin(); _warp != warpData.end(); _warp++) {
            auto warp = _warp.value();
            WarpLocation warpLoc = { warp["m_iToX"], warp["m_iToY"], warp["m_iToZ"], warp["m_iToMapNum"], warp["m_iIsInstance"], warp["m_iLimit_TaskID"], warp["m_iNpcNumber"] };
            int warpID = warp["m_iWarpNumber"];
            NPCManager::Warps[warpID] = warpLoc;
        }

        std::cout << "[INFO] Loaded " << NPCManager::Warps.size() << " Warps" << std::endl;

        // load transport routes and locations
        json transRouteData = xdtData["m_pTransportationTable"]["m_pTransportationData"];
        json transLocData = xdtData["m_pTransportationTable"]["m_pTransportationWarpLocation"];

        for (json::iterator _tLoc = transLocData.begin(); _tLoc != transLocData.end(); _tLoc++) {
            auto tLoc = _tLoc.value();
            TransportLocation transLoc = { tLoc["m_iNPCID"], tLoc["m_iXpos"], tLoc["m_iYpos"], tLoc["m_iZpos"] };
            Transport::Locations[tLoc["m_iLocationID"]] = transLoc;
        }
        std::cout << "[INFO] Loaded " << Transport::Locations.size() << " S.C.A.M.P.E.R. locations" << std::endl;

        for (json::iterator _tRoute = transRouteData.begin(); _tRoute != transRouteData.end(); _tRoute++) {
            auto tRoute = _tRoute.value();
            TransportRoute transRoute = { tRoute["m_iMoveType"], tRoute["m_iStartLocation"], tRoute["m_iEndLocation"],
                tRoute["m_iCost"] , tRoute["m_iSpeed"], tRoute["m_iRouteNum"] };
            Transport::Routes[tRoute["m_iVehicleID"]] = transRoute;
        }
        std::cout << "[INFO] Loaded " << Transport::Routes.size() << " transportation routes" << std::endl;

        // load mission-related data
        json tasks = xdtData["m_pMissionTable"]["m_pMissionData"];

        for (auto _task = tasks.begin(); _task != tasks.end(); _task++) {
            auto task = _task.value();

            // rewards
            if (task["m_iSUReward"] != 0) {
                auto _rew = xdtData["m_pMissionTable"]["m_pRewardData"][(int)task["m_iSUReward"]];
                Reward* rew = new Reward(_rew["m_iMissionRewardID"], _rew["m_iMissionRewarItemType"],
                    _rew["m_iMissionRewardItemID"], _rew["m_iCash"], _rew["m_iFusionMatter"]);

                Missions::Rewards[task["m_iHTaskID"]] = rew;
            }

            // everything else lol. see TaskData comment.
            Missions::Tasks[task["m_iHTaskID"]] = new TaskData(task);
        }

        std::cout << "[INFO] Loaded mission-related data" << std::endl;

        /*
        * load all equipment data. i'm sorry. it has to be done
        * NOTE: please don't change the ordering. it determines the types, since type and equipLoc are used inconsistently
        */
        const char* setNames[11] = { "m_pWeaponItemTable", "m_pShirtsItemTable", "m_pPantsItemTable", "m_pShoesItemTable",
        "m_pHatItemTable", "m_pGlassItemTable", "m_pBackItemTable", "m_pGeneralItemTable", "",
        "m_pChestItemTable", "m_pVehicleItemTable" };
        json itemSet;
        for (int i = 0; i < 11; i++) {
            if (i == 8)
                continue; // there is no type 8, of course

            itemSet = xdtData[setNames[i]]["m_pItemData"];
            for (json::iterator _item = itemSet.begin(); _item != itemSet.end(); _item++) {
                auto item = _item.value();
                int itemID = item["m_iItemNumber"];
                INITSTRUCT(Items::Item, itemData);
                itemData.tradeable = item["m_iTradeAble"] == 1;
                itemData.sellable = item["m_iSellAble"] == 1;
                itemData.buyPrice = item["m_iItemPrice"];
                itemData.sellPrice = item["m_iItemSellPrice"];
                itemData.stackSize = item["m_iStackNumber"];
                if (i != 7 && i != 9) {
                    itemData.rarity = item["m_iRarity"];
                    itemData.level = item["m_iMinReqLev"];
                    itemData.pointDamage = item["m_iPointRat"];
                    itemData.groupDamage = item["m_iGroupRat"];
                    itemData.fireRate = item["m_iDelayTime"];
                    itemData.defense = item["m_iDefenseRat"];
                    itemData.gender = item["m_iReqSex"];
                    itemData.weaponType = item["m_iEquipType"];
                }
                else {
                    itemData.rarity = 1;
                }
                Items::ItemData[std::make_pair(itemID, i)] = itemData;
            }
        }

        std::cout << "[INFO] Loaded " << Items::ItemData.size() << " items" << std::endl;

        // load player limits from m_pAvatarTable.m_pAvatarGrowData

        json growth = xdtData["m_pAvatarTable"]["m_pAvatarGrowData"];

        for (int i = 0; i < 37; i++) {
            Missions::AvatarGrowth[i] = growth[i];
        }

        // load vendor listings
        json listings = xdtData["m_pVendorTable"]["m_pItemData"];

        for (json::iterator _lst = listings.begin(); _lst != listings.end(); _lst++) {
            auto lst = _lst.value();
            VendorListing vListing = { lst["m_iSortNumber"], lst["m_iItemType"], lst["m_iitemID"] };
            Vendors::VendorTables[lst["m_iNpcNumber"]].push_back(vListing);
        }

        std::cout << "[INFO] Loaded " << Vendors::VendorTables.size() << " vendor tables" << std::endl;

        // load crocpot entries
        json crocs = xdtData["m_pCombiningTable"]["m_pCombiningData"];

        for (json::iterator croc = crocs.begin(); croc != crocs.end(); croc++) {
            CrocPotEntry crocEntry = { croc.value()["m_iStatConstant"], croc.value()["m_iLookConstant"], croc.value()["m_fLevelGapStandard"],
                croc.value()["m_fSameGrade"], croc.value()["m_fOneGrade"], croc.value()["m_fTwoGrade"], croc.value()["m_fThreeGrade"] };
            Items::CrocPotTable[croc.value()["m_iLevelGap"]] = crocEntry;
        }

        std::cout << "[INFO] Loaded " << Items::CrocPotTable.size() << " croc pot value sets" << std::endl;

        // load nano info
        json nanoInfo = xdtData["m_pNanoTable"]["m_pNanoData"];
        for (json::iterator _nano = nanoInfo.begin(); _nano != nanoInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoData nanoData;
            nanoData.style = nano["m_iStyle"];
            Nanos::NanoTable[Nanos::NanoTable.size()] = nanoData;
        }

        std::cout << "[INFO] Loaded " << Nanos::NanoTable.size() << " nanos" << std::endl;

        json nanoTuneInfo = xdtData["m_pNanoTable"]["m_pNanoTuneData"];
        for (json::iterator _nano = nanoTuneInfo.begin(); _nano != nanoTuneInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoTuning nanoData;
            nanoData.reqItems = nano["m_iReqItemID"];
            nanoData.reqItemCount = nano["m_iReqItemCount"];
            Nanos::NanoTunings[nano["m_iSkillID"]] = nanoData;
        }

        std::cout << "[INFO] Loaded " << Nanos::NanoTable.size() << " nano tunings" << std::endl;

        // load nano powers
        json skills = xdtData["m_pSkillTable"]["m_pSkillData"];

        for (json::iterator _skills = skills.begin(); _skills != skills.end(); _skills++) {
            auto skills = _skills.value();
            SkillData skillData = { skills["m_iSkillType"], skills["m_iTargetType"], skills["m_iBatteryDrainType"], skills["m_iEffectArea"] };
            for (int i = 0; i < 4; i++) {
                skillData.batteryUse[i] = skills["m_iBatteryDrainUse"][i];
                skillData.durationTime[i] = skills["m_iDurationTime"][i];
                skillData.powerIntensity[i] = skills["m_iValueA"][i];
            }
            Nanos::SkillTable[skills["m_iSkillNumber"]] = skillData;
        }

        std::cout << "[INFO] Loaded " << Nanos::SkillTable.size() << " nano skills" << std::endl;

        // load EP data
        json instances = xdtData["m_pInstanceTable"]["m_pInstanceData"];

        for (json::iterator _instance = instances.begin(); _instance != instances.end(); _instance++) {
            auto instance = _instance.value();
            EPInfo epInfo = { instance["m_iZoneX"], instance["m_iZoneY"], instance["m_iIsEP"], (int)instance["m_ScoreMax"] };
            Racing::EPData[instance["m_iInstanceNameID"]] = epInfo;
        }

        std::cout << "[INFO] Loaded " << Racing::EPData.size() << " instances" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed xdt.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load paths from JSON
 */
static void loadPaths(json& pathData, int32_t* nextId) {
    try {
        // skyway paths
        json pathDataSkyway = pathData["skyway"];
        for (json::iterator skywayPath = pathDataSkyway.begin(); skywayPath != pathDataSkyway.end(); skywayPath++) {
            constructPathSkyway(*skywayPath);
        }
        std::cout << "[INFO] Loaded " << Transport::SkywayPaths.size() << " skyway paths" << std::endl;

        // slider circuit
        json pathDataSlider = pathData["slider"];
        // lerp between keyframes
        std::queue<Vec3> route;
        // initial point
        json::iterator _point = pathDataSlider.begin(); // iterator
        auto point = _point.value();
        Vec3 from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
        int stopTime = point["bStop"] ? SLIDER_STOP_TICKS : 0; // arbitrary stop length
        // remaining points
        for (_point++; _point != pathDataSlider.end(); _point++) { // loop through all point Bs
            point = _point.value();
            for (int i = 0; i < stopTime + 1; i++) { // repeat point if it's a stop
                route.push(from); // add point A to the queue
            }
            Vec3 to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
            // we may need to change this later; right now, the speed is cut before and after stops (no accel)
            float curve = 1;
            if (stopTime > 0) { // point A is a stop
                curve = 0.375f;//2.0f;
            } else if (point["bStop"]) { // point B is a stop
                curve = 0.375f;//0.35f;
            }
            Transport::lerp(&route, from, to, SLIDER_SPEED * curve, 1); // lerp from A to B (arbitrary speed)
            from = to; // update point A
            stopTime = point["bStop"] ? SLIDER_STOP_TICKS : 0; // set stop ticks for next point A
        }
        // Uniform distance calculation
        int passedDistance = 0;
        // initial point
        int pos = 0;
        Vec3 lastPoint = route.front();
        route.pop();
        route.push(lastPoint);
        for (pos = 1; pos < route.size(); pos++) {
            Vec3 point = route.front();
            passedDistance += hypot(point.x - lastPoint.x, point.y - lastPoint.y);
            if (passedDistance >= SLIDER_GAP_SIZE) { // space them out uniformaly
                passedDistance -= SLIDER_GAP_SIZE; // step down
                // spawn a slider
                Bus* slider = new Bus(point.x, point.y, point.z, 0, INSTANCE_OVERWORLD, 1, (*nextId)--);
                NPCManager::NPCs[slider->appearanceData.iNPC_ID] = slider;
                NPCManager::updateNPCPosition(slider->appearanceData.iNPC_ID, slider->x, slider->y, slider->z, INSTANCE_OVERWORLD, 0);
                Transport::NPCQueues[slider->appearanceData.iNPC_ID] = route;
            }
            // rotate
            route.pop();
            route.push(point);
            lastPoint = point;
        }

        // preset npc paths
        json pathDataNPC = pathData["npc"];
        for (json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            json pathVal = npcPath.value();

            std::vector<int32_t> targetIDs;
            std::vector<int32_t> targetTypes;
            std::vector<Vec3> pathPoints;
            int speed = pathVal.find("iBaseSpeed") == pathVal.end() ? NPC_DEFAULT_SPEED : (int)pathVal["iBaseSpeed"];
            int taskID = pathVal.find("iTaskID") == pathVal.end() ? -1 : (int)pathVal["iTaskID"];
            bool relative = pathVal.find("bRelative") == pathVal.end() ? false : (bool)pathVal["bRelative"];
            bool loop = pathVal.find("bLoop") == pathVal.end() ? true : (bool)pathVal["bLoop"]; // loop by default

            // target IDs
            for (json::iterator _tID = pathVal["aNPCIDs"].begin(); _tID != pathVal["aNPCIDs"].end(); _tID++)
                targetIDs.push_back(_tID.value());
            // target types
            for (json::iterator _tType = pathVal["aNPCTypes"].begin(); _tType != pathVal["aNPCTypes"].end(); _tType++)
                targetTypes.push_back(_tType.value());
            // points
            for (json::iterator _point = pathVal["aPoints"].begin(); _point != pathVal["aPoints"].end(); _point++) {
                json point = _point.value();
                for (int stopTicks = 0; stopTicks < (int)point["iStopTicks"] + 1; stopTicks++)
                    pathPoints.push_back({point["iX"], point["iY"], point["iZ"]});
            }

            NPCPath pathTemplate;
            pathTemplate.targetIDs = targetIDs;
            pathTemplate.targetTypes = targetTypes;
            pathTemplate.points = pathPoints;
            pathTemplate.speed = speed;
            pathTemplate.isRelative = relative;
            pathTemplate.isLoop = loop;
            pathTemplate.escortTaskID = taskID;

            Transport::NPCPaths.push_back(pathTemplate);
        }
        std::cout << "[INFO] Loaded " << Transport::NPCPaths.size() << " NPC paths" << std::endl;
        
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed paths.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load drops data from JSON
 * This has to be called after reading xdt because it reffers to ItemData!!!
 */
static void loadDrops(json& dropData) {
    try {
        // CrateDropChances
        json crateDropChances = dropData["CrateDropChances"];
        for (json::iterator _crateDropChance = crateDropChances.begin(); _crateDropChance != crateDropChances.end(); _crateDropChance++) {
            auto crateDropChance = _crateDropChance.value();
            CrateDropChance toAdd = {};

            toAdd.dropChance = (int)crateDropChance["DropChance"];
            toAdd.dropChanceTotal = (int)crateDropChance["DropChanceTotal"];

            json crateWeights = crateDropChance["CrateTypeDropWeights"];
            for (json::iterator _crateWeight = crateWeights.begin(); _crateWeight != crateWeights.end(); _crateWeight++)
                toAdd.crateTypeDropWeights.push_back((int)_crateWeight.value());

            Items::CrateDropChances[(int)crateDropChance["CrateDropChanceID"]] = toAdd;
        }

        // CrateDropTypes
        json crateDropTypes = dropData["CrateDropTypes"];
        for (json::iterator _crateDropType = crateDropTypes.begin(); _crateDropType != crateDropTypes.end(); _crateDropType++) {
            auto crateDropType = _crateDropType.value();
            std::vector<int> toAdd;

            json crateIds = crateDropType["CrateIDs"];
            for (json::iterator _crateId = crateIds.begin(); _crateId != crateIds.end(); _crateId++)
                toAdd.push_back((int)_crateId.value());

            Items::CrateDropTypes[(int)crateDropType["CrateDropTypeID"]] = toAdd;
        }

        // MiscDropChances
        json miscDropChances = dropData["MiscDropChances"];
        for (json::iterator _miscDropChance = miscDropChances.begin(); _miscDropChance != miscDropChances.end(); _miscDropChance++) {
            auto miscDropChance = _miscDropChance.value();

            Items::MiscDropChances[(int)miscDropChance["MiscDropChanceID"]] = {
                (int)miscDropChance["PotionDropChance"],
                (int)miscDropChance["PotionDropChanceTotal"],
                (int)miscDropChance["BoostDropChance"],
                (int)miscDropChance["BoostDropChanceTotal"],
                (int)miscDropChance["TaroDropChance"],
                (int)miscDropChance["TaroDropChanceTotal"],
                (int)miscDropChance["FMDropChance"],
                (int)miscDropChance["FMDropChanceTotal"]
            };
        }

        // MiscDropTypes
        json miscDropTypes = dropData["MiscDropTypes"];
        for (json::iterator _miscDropType = miscDropTypes.begin(); _miscDropType != miscDropTypes.end(); _miscDropType++) {
            auto miscDropType = _miscDropType.value();

            Items::MiscDropTypes[(int)miscDropType["MiscDropTypeID"]] = {
                (int)miscDropType["PotionAmount"],
                (int)miscDropType["BoostAmount"],
                (int)miscDropType["TaroAmount"],
                (int)miscDropType["FMAmount"]
            };
        }

        // MobDrops
        json mobDrops = dropData["MobDrops"];
        for (json::iterator _mobDrop = mobDrops.begin(); _mobDrop != mobDrops.end(); _mobDrop++) {
            auto mobDrop = _mobDrop.value();

            Items::MobDrops[(int)mobDrop["MobDropID"]] = {
                (int)mobDrop["CrateDropChanceID"],
                (int)mobDrop["CrateDropTypeID"],
                (int)mobDrop["MiscDropChanceID"],
                (int)mobDrop["MiscDropTypeID"],
            };
        }

        // Events
        json events = dropData["Events"];
        for (json::iterator _event = events.begin(); _event != events.end(); _event++) {
            auto event = _event.value();

            Items::EventToDropMap[(int)event["EventID"]] = (int)event["MobDropID"];
        }

        // Mobs
        json mobs = dropData["Mobs"];
        for (json::iterator _mob = mobs.begin(); _mob != mobs.end(); _mob++) {
            auto mob = _mob.value();

            Items::MobToDropMap[(int)mob["MobID"]] = (int)mob["MobDropID"];
        }

        // RarityWeights
        json rarityWeights = dropData["RarityWeights"];
        for (json::iterator _rarityWeightsObject = rarityWeights.begin(); _rarityWeightsObject != rarityWeights.end(); _rarityWeightsObject++) {
            auto rarityWeightsObject = _rarityWeightsObject.value();
            std::vector<int> toAdd;

            json weights = rarityWeightsObject["Weights"];
            for (json::iterator _weight = weights.begin(); _weight != weights.end(); _weight++)
                toAdd.push_back((int)_weight.value());

            Items::RarityWeights[(int)rarityWeightsObject["RarityWeightID"]] = toAdd;
        }

        // ItemSets
        json itemSets = dropData["ItemSets"];
        for (json::iterator _itemSet = itemSets.begin(); _itemSet != itemSets.end(); _itemSet++) {
            auto itemSet = _itemSet.value();
            ItemSet toAdd = {};

            toAdd.ignoreRarity = (bool)itemSet["IgnoreRarity"];
            toAdd.ignoreGender = (bool)itemSet["IgnoreGender"];
            toAdd.defaultItemWeight = (int)itemSet["DefaultItemWeight"];

            json alterRarityMap = itemSet["AlterRarityMap"];
            for (json::iterator _alterRarityMapEntry = alterRarityMap.begin(); _alterRarityMapEntry != alterRarityMap.end(); _alterRarityMapEntry++)
                toAdd.alterRarityMap[std::atoi(_alterRarityMapEntry.key().c_str())] = (int)_alterRarityMapEntry.value();

            json alterGenderMap = itemSet["AlterGenderMap"];
            for (json::iterator _alterGenderMapEntry = alterGenderMap.begin(); _alterGenderMapEntry != alterGenderMap.end(); _alterGenderMapEntry++)
                toAdd.alterGenderMap[std::atoi(_alterGenderMapEntry.key().c_str())] = (int)_alterGenderMapEntry.value();

            json alterItemWeightMap = itemSet["AlterItemWeightMap"];
            for (json::iterator _alterItemWeightMapEntry = alterItemWeightMap.begin(); _alterItemWeightMapEntry != alterItemWeightMap.end(); _alterItemWeightMapEntry++)
                toAdd.alterItemWeightMap[std::atoi(_alterItemWeightMapEntry.key().c_str())] = (int)_alterItemWeightMapEntry.value();

            json itemReferenceIds = itemSet["ItemReferenceIDs"];
            for (json::iterator itemReferenceId = itemReferenceIds.begin(); itemReferenceId != itemReferenceIds.end(); itemReferenceId++)
                toAdd.itemReferenceIds.push_back((int)itemReferenceId.value());

            Items::ItemSets[(int)itemSet["ItemSetID"]] = toAdd;
        }

        // Crates
        json crates = dropData["Crates"];
        for (json::iterator _crate = crates.begin(); _crate != crates.end(); _crate++) {
            auto crate = _crate.value();

            Items::Crates[(int)crate["CrateID"]] = {
                (int)crate["ItemSetID"],
                (int)crate["RarityWeightID"]
            };
        }

        // ItemReferences
        json itemReferences = dropData["ItemReferences"];
        for (json::iterator _itemReference = itemReferences.begin(); _itemReference != itemReferences.end(); _itemReference++) {
            auto itemReference = _itemReference.value();

            int itemReferenceId = (int)itemReference["ItemReferenceID"];
            int itemId = (int)itemReference["ItemID"];
            int type = (int)itemReference["Type"];

            // validate and fetch relevant fields as they're loaded
            auto key = std::make_pair(itemId, type);
            if (Items::ItemData.find(key) == Items::ItemData.end()) {
                std::cout << "[WARN] Item-Type pair (" << key.first << ", " << key.second << ") specified by item reference "
                            << itemReferenceId << " was not found, skipping..." << std::endl;
                continue;
            }
            Items::Item& item = Items::ItemData[key];

            Items::ItemReferences[itemReferenceId] = {
                itemId,
                type,
                item.rarity,
                item.gender
            };
        }

#ifdef ACADEMY
        // NanoCapsules
        json capsules = dropData["NanoCapsules"];
        for (json::iterator _capsule = capsules.begin(); _capsule != capsules.end(); _capsule++) {
            auto capsule = _capsule.value();
            Items::NanoCapsules[(int)capsule["CrateID"]] = (int)capsule["Nano"];
        }
#endif

        // Racing rewards
        json racing = dropData["Racing"];
        for (json::iterator _race = racing.begin(); _race != racing.end(); _race++) {
            auto race = _race.value();
            int raceEPID = race["EPID"];

            // find the instance data corresponding to the EPID
            int EPMap = -1;
            for (auto it = Racing::EPData.begin(); it != Racing::EPData.end(); it++) {
                if (it->second.EPID == raceEPID) {
                    EPMap = it->first;
                }
            }

            if (EPMap == -1) { // not found
                std::cout << "[WARN] EP with ID " << raceEPID << " not found, skipping" << std::endl;
                continue;
            }

            // time limit isn't stored in the XDT, so we include it in the reward table instead
            Racing::EPData[EPMap].maxTime = race["TimeLimit"];

            // score cutoffs
            std::vector<int> rankScores;
            for (json::iterator _rankScore = race["RankScores"].begin(); _rankScore != race["RankScores"].end(); _rankScore++) {
                rankScores.push_back((int)_rankScore.value());
            }

            // reward IDs for each rank
            std::vector<int> rankRewards;
            for (json::iterator _rankReward = race["Rewards"].begin(); _rankReward != race["Rewards"].end(); _rankReward++) {
                rankRewards.push_back((int)_rankReward.value());
            }

            if (rankScores.size() != 5 || rankScores.size() != rankRewards.size()) {
                char buff[255];
                sprintf(buff, "Race in EP %d doesn't have exactly 5 score/reward pairs", raceEPID);
                throw TableException(std::string(buff));
            }

            Racing::EPRewards[raceEPID] = std::make_pair(rankScores, rankRewards);
        }

        std::cout << "[INFO] Loaded rewards for " << Racing::EPRewards.size() << " IZ races" << std::endl;

        // CodeItems
        json codes = dropData["CodeItems"];
        for (json::iterator _code = codes.begin(); _code != codes.end(); _code++) {
            auto code = _code.value();
            std::string codeStr = code["Code"];
            std::vector<std::pair<int32_t, int32_t>> itemVector;

            json itemReferenceIds = code["ItemReferenceIDs"];
            for (json::iterator _itemReferenceId = itemReferenceIds.begin(); _itemReferenceId != itemReferenceIds.end(); _itemReferenceId++) {
                int itemReferenceId = (int)_itemReferenceId.value();

                // validate and convert here
                if (Items::ItemReferences.find(itemReferenceId) == Items::ItemReferences.end()) {
                    std::cout << "[WARN] Item reference " << itemReferenceId << " for code "
                              << codeStr << " was not found, skipping..." << std::endl;
                    continue;
                }

                // no need to further check whether this is a real item or not, we already did this!
                ItemReference& itemReference = Items::ItemReferences[itemReferenceId];
                itemVector.push_back(std::make_pair(itemReference.itemId, itemReference.type));
            }

            Items::CodeItems[codeStr] = itemVector;
        }

        std::cout << "[INFO] Loaded " << Items::Crates.size() << " Crates containing "
                  << Items::ItemReferences.size() << " unique items" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed drops.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load eggs from JSON
 */
static void loadEggs(json& eggData, int32_t* nextId) {
    try {
        // EggTypes
        json eggTypes = eggData["EggTypes"];
        for (json::iterator _eggType = eggTypes.begin(); _eggType != eggTypes.end(); _eggType++) {
            auto eggType = _eggType.value();
            EggType toAdd = {};
            toAdd.dropCrateId = (int)eggType["DropCrateId"];
            toAdd.effectId = (int)eggType["EffectId"];
            toAdd.duration = (int)eggType["Duration"];
            toAdd.regen= (int)eggType["Regen"];
            Eggs::EggTypes[(int)eggType["Id"]] = toAdd;
        }

        // Egg instances
        auto eggs = eggData["Eggs"];
        int eggCount = 0;
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)--;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            eggCount++;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
        }

        std::cout << "[INFO] Loaded " << eggCount << " eggs" <<std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed eggs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/* 
 * Load gruntwork output, if it exists
 */
static void loadGruntworkPre(json& gruntwork, int32_t* nextId) {

    try {
        auto paths = gruntwork["paths"];
        for (auto _path = paths.begin(); _path != paths.end(); _path++) {
            auto path = _path.value();

            std::vector<int32_t> targetIDs;
            std::vector<int32_t> targetTypes; // target types are not exportable from gw, but load them anyway
            std::vector<Vec3> pathPoints;
            int speed = (int)path["iBaseSpeed"];
            int taskID = (int)path["iTaskID"];
            bool relative = (bool)path["bRelative"];
            bool loop = (bool)path["bLoop"];

            // target IDs
            for (json::iterator _tID = path["aNPCIDs"].begin(); _tID != path["aNPCIDs"].end(); _tID++)
                targetIDs.push_back(_tID.value());
            // target types
            for (json::iterator _tType = path["aNPCTypes"].begin(); _tType != path["aNPCTypes"].end(); _tType++)
                targetTypes.push_back(_tType.value());
            // points
            for (json::iterator _point = path["aPoints"].begin(); _point != path["aPoints"].end(); _point++) {
                json point = _point.value();
                for (int stopTicks = 0; stopTicks < (int)point["iStopTicks"] + 1; stopTicks++)
                    pathPoints.push_back({ point["iX"], point["iY"], point["iZ"] });
            }

            NPCPath pathTemplate;
            pathTemplate.targetIDs = targetIDs;
            pathTemplate.targetTypes = targetTypes;
            pathTemplate.points = pathPoints;
            pathTemplate.speed = speed;
            pathTemplate.isRelative = relative;
            pathTemplate.escortTaskID = taskID;
            pathTemplate.isLoop = loop;

            Transport::NPCPaths.push_back(pathTemplate);
            TableData::FinishedNPCPaths.push_back(pathTemplate); // keep in gruntwork
        }

        std::cout << "[INFO] Loaded gruntwork.json (pre)" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed gruntwork.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

static void loadGruntworkPost(json& gruntwork, int32_t* nextId) {

    if (gruntwork.is_null()) return;

    try {
        // skyway paths
        auto skyway = gruntwork["skyway"];
        for (auto _route = skyway.begin(); _route != skyway.end(); _route++) {
            auto route = _route.value();
            std::vector<Vec3> points;

            for (auto _point = route["points"].begin(); _point != route["points"].end(); _point++) {
                auto point = _point.value();
                points.push_back(Vec3{point["x"], point["y"], point["z"]});
            }

            RunningSkywayRoutes[(int)route["iRouteID"]] = points;
        }

        // npc rotations
        auto npcRot = gruntwork["rotations"];
        for (auto _rot = npcRot.begin(); _rot != npcRot.end(); _rot++) {
            int32_t npcID = _rot.value()["iNPCID"];
            int angle = _rot.value()["iAngle"];
            if (NPCManager::NPCs.find(npcID) == NPCManager::NPCs.end())
                continue; // NPC not found
            BaseNPC* npc = NPCManager::NPCs[npcID];
            npc->appearanceData.iAngle = angle;

            RunningNPCRotations[npcID] = angle;
        }

        // npc map numbers
        auto npcMap = gruntwork["instances"];
        for (auto _map = npcMap.begin(); _map != npcMap.end(); _map++) {
            int32_t npcID = _map.value()["iNPCID"];
            uint64_t instanceID = _map.value()["iMapNum"];
            if (NPCManager::NPCs.find(npcID) == NPCManager::NPCs.end())
                continue; // NPC not found
            BaseNPC* npc = NPCManager::NPCs[npcID];
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->x, npc->y,
                npc->z, instanceID, npc->appearanceData.iAngle);

            RunningNPCMapNumbers[npcID] = instanceID;
        }

        // mobs
        auto mobs = gruntwork["mobs"];
        for (auto _mob = mobs.begin(); _mob != mobs.end(); _mob++) {
            auto mob = _mob.value();
            BaseNPC *npc;
            int id = (*nextId)--;
            uint64_t instanceID = mob.find("iMapNum") == mob.end() ? INSTANCE_OVERWORLD : (int)mob["iMapNum"];

            if (NPCManager::NPCData[(int)mob["iNPCType"]]["m_iTeam"] == 2) {
                npc = new Mob(mob["iX"], mob["iY"], mob["iZ"], instanceID, mob["iNPCType"],
                    NPCManager::NPCData[(int)mob["iNPCType"]], id);

                // re-enable respawning
                ((Mob*)npc)->summoned = false;
            } else {
                npc = new BaseNPC(mob["iX"], mob["iY"], mob["iZ"], mob["iAngle"], instanceID, mob["iNPCType"], id);
            }

            NPCManager::NPCs[npc->appearanceData.iNPC_ID] = npc;
            RunningMobs[npc->appearanceData.iNPC_ID] = npc;
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, mob["iX"], mob["iY"], mob["iZ"], instanceID, mob["iAngle"]);
        }

        // mob groups
        auto groups = gruntwork["groups"];
        for (auto _group = groups.begin(); _group != groups.end(); _group++) {
            auto leader = _group.value();
            auto td = NPCManager::NPCData[(int)leader["iNPCType"]];
            uint64_t instanceID = leader.find("iMapNum") == leader.end() ? INSTANCE_OVERWORLD : (int)leader["iMapNum"];

            Mob* tmp = new Mob(leader["iX"], leader["iY"], leader["iZ"], leader["iAngle"], instanceID, leader["iNPCType"], td, *nextId);

            // re-enable respawning
            ((Mob*)tmp)->summoned = false;

            NPCManager::NPCs[*nextId] = tmp;
            NPCManager::updateNPCPosition(*nextId, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            tmp->groupLeader = *nextId;

            (*nextId)--;

            auto followers = leader["aFollowers"];
            if (followers.size() < 5) {
                int followerCount = 0;
                for (json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, *nextId);

                    // re-enable respawning
                    ((Mob*)tmp)->summoned = false;

                    NPCManager::NPCs[*nextId] = tmpFol;
                    NPCManager::updateNPCPosition(*nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = *nextId;

                    (*nextId)--;
                }
            }
            else {
                std::cout << "[WARN] Mob group leader with ID " << *nextId << " has too many followers (" << followers.size() << ")\n";
            }

            RunningGroups[tmp->appearanceData.iNPC_ID] = tmp; // store as running
        }

        auto eggs = gruntwork["eggs"];
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)--;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
            RunningEggs[id] = addEgg;
        }

        std::cout << "[INFO] Loaded gruntwork.json (post)" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed gruntwork.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load NPCs from JSON
 */
static void loadNPCs(json& npcData) {
    try {
        npcData = npcData["NPCs"];
        for (json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            int npcID = std::strtol(_npc.key().c_str(), nullptr, 10); // parse ID string to integer
            npcID += NPC_ID_OFFSET;
            int instanceID = npc.find("iMapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["iMapNum"];
            int type = (int)npc["iNPCType"];
            if (NPCManager::NPCData[type].is_null()) {
                std::cout << "[WARN] NPC type " << type << " not found; skipping (json#" << _npc.key() << ")" << std::endl;
                continue;
            }
#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (npc["iX"] > 512000 && npc["iY"] < 256000)
                continue;
#endif
            BaseNPC* tmp = new BaseNPC(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, type, npcID);

            NPCManager::NPCs[npcID] = tmp;
            NPCManager::updateNPCPosition(npcID, npc["iX"], npc["iY"], npc["iZ"], instanceID, npc["iAngle"]);

            if (type == 641 || type == 642)
                NPCManager::RespawnPoints.push_back({ npc["iX"], npc["iY"], ((int)npc["iZ"]) + RESURRECT_HEIGHT, instanceID });

            // see if any paths target this NPC
            NPCPath* npcPath = Transport::findApplicablePath(npcID, type);
            if (npcPath != nullptr) {
                //std::cout << "[INFO] Found path for NPC " << npcID << std::endl;
                Transport::constructPathNPC(npcID, npcPath);
            }
        }
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load mobs from JSON
 */
static void loadMobs(json& npcData, int32_t* nextId) {
    try {
        json groupData = npcData["groups"];
        npcData = npcData["mobs"];

        // single mobs
        for (json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            int npcID = std::strtol(_npc.key().c_str(), nullptr, 10); // parse ID string to integer
            npcID += MOB_ID_OFFSET;
            int type = (int)npc["iNPCType"];
            if (NPCManager::NPCData[type].is_null()) {
                std::cout << "[WARN] NPC type " << type << " not found; skipping (json#" << _npc.key() << ")" << std::endl;
                continue;
            }
            auto td = NPCManager::NPCData[type];
            uint64_t instanceID = npc.find("iMapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["iMapNum"];

#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (npc["iX"] > 512000 && npc["iY"] < 256000)
                continue;
#endif

            Mob* tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, npc["iNPCType"], td, npcID);

            NPCManager::NPCs[npcID] = tmp;
            NPCManager::updateNPCPosition(npcID, npc["iX"], npc["iY"], npc["iZ"], instanceID, npc["iAngle"]);

            // see if any paths target this mob
            NPCPath* npcPath = Transport::findApplicablePath(npcID, npc["iNPCType"]);
            if (npcPath != nullptr) {
                //std::cout << "[INFO] Found path for mob " << npcID << std::endl;
                Transport::constructPathNPC(npcID, npcPath);
            }
        }

        // mob groups
        // single mobs (have static IDs)
        for (json::iterator _group = groupData.begin(); _group != groupData.end(); _group++) {
            auto leader = _group.value();
            int leadID = std::strtol(_group.key().c_str(), nullptr, 10); // parse ID string to integer
            leadID += MOB_GROUP_ID_OFFSET;
            auto td = NPCManager::NPCData[(int)leader["iNPCType"]];
            uint64_t instanceID = leader.find("iMapNum") == leader.end() ? INSTANCE_OVERWORLD : (int)leader["iMapNum"];
            auto followers = leader["aFollowers"];

#ifdef ACADEMY
            // do not spawn NPCs in the future
            if (leader["iX"] > 512000 && leader["iY"] < 256000)
                continue;
#endif

            Mob* tmp = new Mob(leader["iX"], leader["iY"], leader["iZ"], leader["iAngle"], instanceID, leader["iNPCType"], td, leadID);

            NPCManager::NPCs[leadID] = tmp;
            NPCManager::updateNPCPosition(leadID, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            // see if any paths target this group leader
            NPCPath* npcPath = Transport::findApplicablePath(leadID, leader["iNPCType"]);
            if (npcPath != nullptr) {
                //std::cout << "[INFO] Found path for mob " << leadID << std::endl;
                Transport::constructPathNPC(leadID, npcPath);
            }

            tmp->groupLeader = leadID;

            // followers (have dynamic IDs)
            if (followers.size() < 5) {
                int followerCount = 0;
                for (json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, *nextId);

                    NPCManager::NPCs[*nextId] = tmpFol;
                    NPCManager::updateNPCPosition(*nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = *nextId;

                    (*nextId)--;
                }
            }
            else {
                std::cout << "[WARN] Mob group leader with ID " << leadID << " has too many followers (" << followers.size() << ")\n";
            }
        }

        std::cout << "[INFO] Loaded " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed mobs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Transform `base` based on the value of `patch`.
 * Parameters must be of the same type and must not be null.
 * Array <- Array: All elements in patch array get added to base array.
 * Object <- Object: Combine properties recursively and save to base.
 * All other types <- Same type: Base value gets overwritten by patch value.
 */
static void patchJSON(json* base, json* patch) {
    if (patch->is_null() || base->is_null())
        return; // no nulls allowed!!

    if ((json::value_t)(*base) != (json::value_t)(*patch)) {
        // verify type mismatch. unsigned <-> integer is ok.
        if (!((base->is_number_integer() && patch->is_number_unsigned())
            || (base->is_number_unsigned() && patch->is_number_integer())))
            return;
    }

    // case 1: type is array
    if (patch->is_array()) {
        // loop through all array elements
        for (json::iterator _element = patch->begin(); _element != patch->end(); _element++)
            base->push_back(*_element); // add element to base
    }
    // case 2: type is object
    else if (patch->is_object()) {
        // loop through all object properties
        for (json::iterator _prop = patch->begin(); _prop != patch->end(); _prop++) {
            std::string key = _prop.key(); // static identifier
            json* valLoc = &(*_prop); // pointer to json data

            // special casing for forced replacement.
            // the ! is stripped, then the property is forcibly replaced without a recursive call
            // that means no type checking, so use at your own risk
            if (key.c_str()[0] == '!') {
                key = key.substr(1, key.length() - 1);
                (*base)[key] = *valLoc;
                continue;
            }

            // search for matching property in base object
            json::iterator _match = base->find(key);
            if (_match != base->end()) {
                // match found
                if (valLoc->is_null()) // prop value is null; erase match
                    base->erase(key);
                else { // combine objects
                    json* match = &(*_match);
                    patchJSON(match, valLoc);
                }
            } else {
                // no match found; add the property to the base object
                (*base)[key] = *valLoc;
            }
        }
    }
    // case 3: all other types
    else {
        *base = *patch; // complete overwrite
    }
}

void TableData::init() {
    int32_t nextId = INT32_MAX; // next dynamic ID to hand out

    // base JSON tables
    json xdt, paths, drops, eggs, npcs, mobs, gruntwork;
    std::pair<json*, std::string> tables[7] = {
        std::make_pair(&xdt, settings::XDTJSON),
        std::make_pair(&paths, settings::PATHJSON),
        std::make_pair(&drops, settings::DROPSJSON),
        std::make_pair(&eggs, settings::EGGSJSON),
        std::make_pair(&npcs, settings::NPCJSON),
        std::make_pair(&mobs, settings::MOBJSON),
        std::make_pair(&gruntwork, settings::GRUNTWORKJSON)
    };

    // load JSON data into tables
    std::ifstream fstream;
    for (int i = 0; i < 7; i++) {
        std::pair<json*, std::string>& table = tables[i];
        fstream.open(settings::TDATADIR + table.second); // open file
        if (!fstream.fail()) {
            fstream >> *table.first; // load file contents into table
        } else {
            if (table.first != &gruntwork) { // gruntwork isn't critical
                std::cerr << "[FATAL] Critical tdata file missing: " << settings::TDATADIR << table.second << std::endl;
                exit(1);
            }
        }
        fstream.close();

        // patching: load each patch directory specified in the config file

        // split config field into individual patch entries
        std::stringstream ss(settings::ENABLEDPATCHES);
        std::istream_iterator<std::string> begin(ss);
        std::istream_iterator<std::string> end;

        json patch;
        for (auto it = begin; it != end; it++) {
            // this is the theoretical path of a corresponding patch for this file
            std::string patchModuleName = *it;
            std::string patchFile = settings::PATCHDIR + patchModuleName + "/" + table.second;
            try {
                fstream.open(patchFile);
                fstream >> patch; // load into temporary json object
                std::cout << "[INFO] Patching " << patchFile << std::endl;
                patchJSON(table.first, &patch); // patch
                fstream.close();
            } catch (const std::exception& err) {
                // no-op
            }
        }
    }

    // fetch data from patched tables and load them appropriately
    // note: the order of these is important
    std::cout << "[INFO] Loading tabledata..." << std::endl;
    loadXDT(xdt);
    loadGruntworkPre(gruntwork, &nextId);
    loadPaths(paths, &nextId);
    loadNPCs(npcs);
    loadMobs(mobs, &nextId);
    loadDrops(drops);
    loadEggs(eggs, &nextId);
    loadGruntworkPost(gruntwork, &nextId);

    NPCManager::nextId = nextId;
}

/*
 * Write gruntwork output to file
 */
void TableData::flush() {
    std::ofstream file(settings::TDATADIR + settings::GRUNTWORKJSON);
    json gruntwork;

    for (auto& pair : RunningSkywayRoutes) {
        json route;

        route["iRouteID"] = (int)pair.first;
        route["iMonkeySpeed"] = 1500;

        std::cout << "serializing mss route " << (int)pair.first << std::endl;
        for (Vec3& point : pair.second) {
            json tmp;

            tmp["x"] = point.x;
            tmp["y"] = point.y;
            tmp["z"] = point.z;

            route["points"].push_back(tmp);
        }

        gruntwork["skyway"].push_back(route);
    }

    for (auto& pair : RunningNPCRotations) {
        json rotation;

        rotation["iNPCID"] = (int)pair.first;
        rotation["iAngle"] = pair.second;

        gruntwork["rotations"].push_back(rotation);
    }

    for (auto& pair : RunningNPCMapNumbers) {
        json mapNumber;

        mapNumber["iNPCID"] = (int)pair.first;
        mapNumber["iMapNum"] = pair.second;

        gruntwork["instances"].push_back(mapNumber);
    }

    for (auto& pair : RunningMobs) {
        json mob;
        BaseNPC *npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        if (npc->type == EntityType::MOB) {
            Mob *m = (Mob*)npc;
            x = m->spawnX;
            y = m->spawnY;
            z = m->spawnZ;
        } else {
            x = npc->x;
            y = npc->y;
            z = npc->z;
        }

        // NOTE: this format deviates slightly from the one in mobs.json
        mob["iNPCType"] = (int)npc->appearanceData.iNPCType;
        mob["iX"] = x;
        mob["iY"] = y;
        mob["iZ"] = z;
        mob["iMapNum"] = MAPNUM(npc->instanceID);
        // this is a bit imperfect, since this is a live angle, not a spawn angle so it'll change often, but eh
        mob["iAngle"] = npc->appearanceData.iAngle;

        // it's called mobs, but really it's everything
        gruntwork["mobs"].push_back(mob);
    }

    for (auto& pair : RunningGroups) {
        json mob;
        BaseNPC* npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        std::vector<Mob*> followers;
        if (npc->type == EntityType::MOB) {
            Mob* m = (Mob*)npc;
            x = m->spawnX;
            y = m->spawnY;
            z = m->spawnZ;
            if (m->groupLeader != m->appearanceData.iNPC_ID) { // make sure this is a leader
                std::cout << "[WARN] Non-leader mob found in running groups; ignoring\n";
                continue;
            }

            // add follower data to vector; go until OOB or until follower ID is 0
            for (int i = 0; i < 4 && m->groupMember[i] > 0; i++) {
                if (NPCManager::NPCs.find(m->groupMember[i]) == NPCManager::NPCs.end() || NPCManager::NPCs[m->groupMember[i]]->type != EntityType::MOB) {
                    std::cout << "[WARN] Follower with ID " << m->groupMember[i] << " not found; skipping\n";
                    continue;
                }
                followers.push_back((Mob*)NPCManager::NPCs[m->groupMember[i]]);
            }
        }
        else {
            x = npc->x;
            y = npc->y;
            z = npc->z;
        }

        // NOTE: this format deviates slightly from the one in mobs.json
        mob["iNPCType"] = (int)npc->appearanceData.iNPCType;
        mob["iX"] = x;
        mob["iY"] = y;
        mob["iZ"] = z;
        mob["iMapNum"] = MAPNUM(npc->instanceID);
        // this is a bit imperfect, since this is a live angle, not a spawn angle so it'll change often, but eh
        mob["iAngle"] = npc->appearanceData.iAngle;

        // followers
        while (followers.size() > 0) {
            Mob* follower = followers.back();
            followers.pop_back(); // remove from vector

            // populate JSON entry
            json fol;
            fol["iNPCType"] = follower->appearanceData.iNPCType;
            fol["iOffsetX"] = follower->offsetX;
            fol["iOffsetY"] = follower->offsetY;

            mob["aFollowers"].push_back(fol); // add to follower array
        }

        // it's called mobs, but really it's everything
        gruntwork["groups"].push_back(mob);
    }

    for (auto& pair : RunningEggs) {
        json egg;
        BaseNPC* npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;
        // we can trust that if it exists, it probably is indeed an egg

        egg["iX"] = npc->x;
        egg["iY"] = npc->y;
        egg["iZ"] = npc->z;
        int mapnum = MAPNUM(npc->instanceID);
        if (mapnum != 0)
            egg["iMapNum"] = mapnum;
        egg["iType"] = npc->appearanceData.iNPCType;

        gruntwork["eggs"].push_back(egg);
    }

    for (auto& path : FinishedNPCPaths) {
        json pathObj;
        json points;
        json targetIDs;
        json targetTypes;

        for (Vec3& coord : path.points) {
            json point;
            point["iX"] = coord.x;
            point["iY"] = coord.y;
            point["iZ"] = coord.z;
            point["iStopTicks"] = 0;
            points.push_back(point);
        }

        for (int32_t tID : path.targetIDs)
            targetIDs.push_back(tID);
        for (int32_t tType : path.targetTypes)
            targetTypes.push_back(tType);
        
        pathObj["iBaseSpeed"] = path.speed;
        pathObj["iTaskID"] = path.escortTaskID;
        pathObj["bRelative"] = path.isRelative;
        pathObj["aPoints"] = points;
        pathObj["bLoop"] = path.isLoop;

        // don't write 'null' if there aren't any targets
        if(targetIDs.size() > 0)
            pathObj["aNPCIDs"] = targetIDs;
        if (targetTypes.size() > 0)
            pathObj["aNPCTypes"] = targetTypes;

        gruntwork["paths"].push_back(pathObj);
    }

    file << gruntwork << std::endl;
}
