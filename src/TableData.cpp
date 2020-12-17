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

std::map<int32_t, std::vector<WarpLocation>> TableData::RunningSkywayRoutes;
std::map<int32_t, int> TableData::RunningNPCRotations;
std::map<int32_t, int> TableData::RunningNPCMapNumbers;
std::map<int32_t, BaseNPC*> TableData::RunningMobs;
std::map<int32_t, BaseNPC*> TableData::RunningGroups;
std::map<int32_t, BaseNPC*> TableData::RunningEggs;

class TableException : public std::exception {
public:
    std::string msg;

    TableException(std::string m) : std::exception() { msg = m; }

    const char *what() const throw() { return msg.c_str(); }
};

void TableData::init() {
    int32_t nextId = 0;

    // load NPCs from NPC.json
    try {
        std::ifstream inFile(settings::NPCJSON);
        nlohmann::json npcData;

        // read file into json
        inFile >> npcData;
        npcData = npcData["NPCs"];
        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            int instanceID = npc.find("mapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["mapNum"];
            BaseNPC *tmp = new BaseNPC(npc["x"], npc["y"], npc["z"], npc["angle"], instanceID, npc["id"], nextId);

            NPCManager::NPCs[nextId] = tmp;
            NPCManager::updateNPCPosition(nextId, npc["x"], npc["y"], npc["z"], instanceID, npc["angle"]);
            nextId++;

            if (npc["id"] == 641 || npc["id"] == 642)
                NPCManager::RespawnPoints.push_back({ npc["x"], npc["y"], ((int)npc["z"]) + RESURRECT_HEIGHT, instanceID });
        }
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed NPCs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

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
            WarpLocation warpLoc = { warp["m_iToX"], warp["m_iToY"], warp["m_iToZ"], warp["m_iToMapNum"], warp["m_iIsInstance"], warp["m_iLimit_TaskID"], warp["m_iNpcNumber"] };
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

        /*
        * load all equipment data. i'm sorry. it has to be done
        * NOTE: please don't change the ordering. it determines the types, since type and equipLoc are used inconsistently
        */
        const char* setNames[11] = { "m_pWeaponItemTable", "m_pShirtsItemTable", "m_pPantsItemTable", "m_pShoesItemTable",
        "m_pHatItemTable", "m_pGlassItemTable", "m_pBackItemTable", "m_pGeneralItemTable", "",
        "m_pChestItemTable", "m_pVehicleItemTable" };
        nlohmann::json itemSet;
        for (int i = 0; i < 11; i++) {
            if (i == 8)
                continue; // there is no type 8, of course

            itemSet = xdtData[setNames[i]]["m_pItemData"];
            for (nlohmann::json::iterator _item = itemSet.begin(); _item != itemSet.end(); _item++) {
                auto item = _item.value();
                int itemID = item["m_iItemNumber"];
                INITSTRUCT(ItemManager::Item, itemData);
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
                    itemData.defense = item["m_iDefenseRat"];
                    itemData.gender = item["m_iReqSex"];
                } else {
                    itemData.rarity = 1;
                }
                ItemManager::ItemData[std::make_pair(itemID, i)] = itemData;
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

        // load nano info
        nlohmann::json nanoInfo = xdtData["m_pNanoTable"]["m_pNanoData"];
        for (nlohmann::json::iterator _nano = nanoInfo.begin(); _nano != nanoInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoData nanoData;
            nanoData.style = nano["m_iStyle"];
            NanoManager::NanoTable[nano["m_iNanoNumber"]] = nanoData;
        }

        std::cout << "[INFO] Loaded " << NanoManager::NanoTable.size() << " nanos" << std::endl;

        nlohmann::json nanoTuneInfo = xdtData["m_pNanoTable"]["m_pNanoTuneData"];
        for (nlohmann::json::iterator _nano = nanoTuneInfo.begin(); _nano != nanoTuneInfo.end(); _nano++) {
            auto nano = _nano.value();
            NanoTuning nanoData;
            nanoData.reqItems = nano["m_iReqItemID"];
            nanoData.reqItemCount = nano["m_iReqItemCount"];
            NanoManager::NanoTunings[nano["m_iSkillID"]] = nanoData;
        }

        std::cout << "[INFO] Loaded " << NanoManager::NanoTable.size() << " nano tunings" << std::endl;

        // load nano powers
        nlohmann::json skills = xdtData["m_pSkillTable"]["m_pSkillData"];

        for (nlohmann::json::iterator _skills = skills.begin(); _skills != skills.end(); _skills++) {
            auto skills = _skills.value();
            SkillData skillData = {skills["m_iSkillType"], skills["m_iTargetType"], skills["m_iBatteryDrainType"], skills["m_iEffectArea"]};
            for (int i = 0; i < 4; i++) {
                skillData.batteryUse[i] = skills["m_iBatteryDrainUse"][i];
                skillData.durationTime[i] = skills["m_iDurationTime"][i];
                skillData.powerIntensity[i] = skills["m_iValueA"][i];
            }
            NanoManager::SkillTable[skills["m_iSkillNumber"]] = skillData;
        }

        std::cout << "[INFO] Loaded " << NanoManager::SkillTable.size() << " nano skills" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed xdt.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

    // load mobs
    try {
        std::ifstream inFile(settings::MOBJSON);
        nlohmann::json npcData, groupData;

        // read file into json
        inFile >> npcData;
        groupData = npcData["groups"];
        npcData = npcData["mobs"];

        // single mobs
        for (nlohmann::json::iterator _npc = npcData.begin(); _npc != npcData.end(); _npc++) {
            auto npc = _npc.value();
            auto td = NPCManager::NPCData[(int)npc["iNPCType"]];
            uint64_t instanceID = npc.find("iMapNum") == npc.end() ? INSTANCE_OVERWORLD : (int)npc["iMapNum"];

            Mob *tmp = new Mob(npc["iX"], npc["iY"], npc["iZ"], npc["iAngle"], instanceID, npc["iNPCType"], td, nextId);

            NPCManager::NPCs[nextId] = tmp;
            MobManager::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            NPCManager::updateNPCPosition(nextId, npc["iX"], npc["iY"], npc["iZ"], instanceID, npc["iAngle"]);

            nextId++;
        }

        // mob groups
        // single mobs
        for (nlohmann::json::iterator _group = groupData.begin(); _group != groupData.end(); _group++) {
            auto leader = _group.value();
            auto td = NPCManager::NPCData[(int)leader["iNPCType"]];
            uint64_t instanceID = leader.find("iMapNum") == leader.end() ? INSTANCE_OVERWORLD : (int)leader["iMapNum"];

            Mob* tmp = new Mob(leader["iX"], leader["iY"], leader["iZ"], leader["iAngle"], instanceID, leader["iNPCType"], td, nextId);

            NPCManager::NPCs[nextId] = tmp;
            MobManager::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
            NPCManager::updateNPCPosition(nextId, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            tmp->groupLeader = nextId;

            nextId++;

            auto followers = leader["aFollowers"];
            if (followers.size() < 5) {
                int followerCount = 0;
                for (nlohmann::json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, nextId);

                    NPCManager::NPCs[nextId] = tmpFol;
                    MobManager::Mobs[nextId] = (Mob*)NPCManager::NPCs[nextId];
                    NPCManager::updateNPCPosition(nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = nextId;

                    nextId++;
                }
            } else {
                std::cout << "[WARN] Mob group leader with ID " << nextId << " has too many followers (" << followers.size() << ")\n";
            }
        }

        std::cout << "[INFO] Populated " << NPCManager::NPCs.size() << " NPCs" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed mobs.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }

    loadDrops();

    loadEggs(&nextId);

    loadPaths(&nextId); // load paths

    loadGruntwork(&nextId);

    NPCManager::nextId = nextId;
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
        nlohmann::json pathDataSlider = pathData["slider"];
        // lerp between keyframes
        std::queue<WarpLocation> route;
        // initial point
        nlohmann::json::iterator _point = pathDataSlider.begin(); // iterator
        auto point = _point.value();
        WarpLocation from = { point["iX"] , point["iY"] , point["iZ"] }; // point A coords
        int stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0; // arbitrary stop length
        // remaining points
        for (_point++; _point != pathDataSlider.end(); _point++) { // loop through all point Bs
            point = _point.value();
            for (int i = 0; i < stopTime + 1; i++) { // repeat point if it's a stop
                route.push(from); // add point A to the queue
            }
            WarpLocation to = { point["iX"] , point["iY"] , point["iZ"] }; // point B coords
            // we may need to change this later; right now, the speed is cut before and after stops (no accel)
            float curve = 1;
            if (stopTime > 0) { // point A is a stop
                curve = 0.375f;//2.0f;
            } else if (point["stop"]) { // point B is a stop
                curve = 0.375f;//0.35f;
            }
            int preLerp = route.size();
            TransportManager::lerp(&route, from, to, SLIDER_SPEED * curve, 1); // lerp from A to B (arbitrary speed)
            int postLerp = route.size() - preLerp;
            from = to; // update point A
            stopTime = point["stop"] ? SLIDER_STOP_TICKS : 0; // set stop ticks for next point A
        }
        //
        int l = route.size();
        int numSlidersApprox = 16; // maybe add a config option for this?
        for (int pos = 0; pos < l; pos++) {
            WarpLocation point = route.front();
            if (pos % (l / numSlidersApprox) == 0) { // space them out uniformaly
                // spawn a slider
                BaseNPC* slider = new BaseNPC(point.x, point.y, point.z, 0, INSTANCE_OVERWORLD, 1, (*nextId)++, NPC_BUS);
                NPCManager::NPCs[slider->appearanceData.iNPC_ID] = slider;
                NPCManager::updateNPCPosition(slider->appearanceData.iNPC_ID, slider->appearanceData.iX, slider->appearanceData.iY, slider->appearanceData.iZ, INSTANCE_OVERWORLD, 0);
                TransportManager::NPCQueues[slider->appearanceData.iNPC_ID] = route;
            }
            // rotate
            route.pop();
            route.push(point);
        }

        // npc paths
        nlohmann::json pathDataNPC = pathData["npc"];
        /*
        for (nlohmann::json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            constructPathNPC(npcPath);
        }
        */

        // mob paths
        pathDataNPC = pathData["mob"];
        for (nlohmann::json::iterator npcPath = pathDataNPC.begin(); npcPath != pathDataNPC.end(); npcPath++) {
            for (auto& pair : MobManager::Mobs) {
                if (pair.second->appearanceData.iNPCType == npcPath.value()["iNPCType"]) {
                    std::cout << "[INFO] Using static path for mob " << pair.second->appearanceData.iNPCType << " with ID " << pair.first << std::endl;

                    auto firstPoint = npcPath.value()["points"][0];
                    if (firstPoint["iX"] != pair.second->spawnX || firstPoint["iY"] != pair.second->spawnY) {
                        std::cout << "[FATAL] The first point of the route for mob " << pair.first <<
                            " (type " << pair.second->appearanceData.iNPCType << ") does not correspond with its spawn point." << std::endl;
                        exit(1);
                    }

                    constructPathNPC(npcPath, pair.first);
                    pair.second->staticPath = true;
                    break; // only one NPC per path
                }
            }
        }
        std::cout << "[INFO] Loaded " << TransportManager::NPCQueues.size() << " NPC paths" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed paths.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

/*
 * Load drops data from JSON.
 * This has to be called after reading xdt because it reffers to ItemData!!!
 */
void TableData::loadDrops() {
    try {
        std::ifstream inFile(settings::DROPSJSON);
        nlohmann::json dropData;

        // read file into json
        inFile >> dropData;

        // MobDropChances
        nlohmann::json mobDropChances = dropData["MobDropChances"];
        for (nlohmann::json::iterator _dropChance = mobDropChances.begin(); _dropChance != mobDropChances.end(); _dropChance++) {
            auto dropChance = _dropChance.value();
            MobDropChance toAdd = {};
            toAdd.dropChance = (int)dropChance["DropChance"];
            for (nlohmann::json::iterator _cratesRatio = dropChance["CratesRatio"].begin(); _cratesRatio != dropChance["CratesRatio"].end(); _cratesRatio++) {
                toAdd.cratesRatio.push_back((int)_cratesRatio.value());
            }
            MobManager::MobDropChances[(int)dropChance["Type"]] = toAdd;
        }

        // MobDrops
        nlohmann::json mobDrops = dropData["MobDrops"];
        for (nlohmann::json::iterator _drop = mobDrops.begin(); _drop != mobDrops.end(); _drop++) {
            auto drop = _drop.value();
            MobDrop toAdd = {};
            for (nlohmann::json::iterator _crates = drop["CrateIDs"].begin(); _crates != drop["CrateIDs"].end(); _crates++) {
                toAdd.crateIDs.push_back((int)_crates.value());
            }

            toAdd.dropChanceType = (int)drop["DropChance"];
            // Check if DropChance exists
            if (MobManager::MobDropChances.find(toAdd.dropChanceType) == MobManager::MobDropChances.end()) {
                throw TableException(" MobDropChance not found: " + std::to_string((toAdd.dropChanceType)));
            }
            // Check if number of crates is correct
            if (!(MobManager::MobDropChances[(int)drop["DropChance"]].cratesRatio.size() == toAdd.crateIDs.size())) {
                throw TableException(" DropType " + std::to_string((int)drop["DropType"]) + " contains invalid number of crates");
            }

            toAdd.taros = (int)drop["Taros"];
            toAdd.fm = (int)drop["FM"];
            toAdd.boosts = (int)drop["Boosts"];
            MobManager::MobDrops[(int)drop["DropType"]] = toAdd;
        }

        std::cout << "[INFO] Loaded " << MobManager::MobDrops.size() << " Mob Drop Types"<<  std::endl;

        // Rarity Ratios
        nlohmann::json rarities = dropData["RarityRatios"];
        for (nlohmann::json::iterator _rarity = rarities.begin(); _rarity != rarities.end(); _rarity++) {
            auto rarity = _rarity.value();
            std::vector<int> toAdd;
            for (nlohmann::json::iterator _ratio = rarity["Ratio"].begin(); _ratio != rarity["Ratio"].end(); _ratio++){
                toAdd.push_back((int)_ratio.value());
            }
            ItemManager::RarityRatios[(int)rarity["Type"]] = toAdd;
        }

        // Crates
        nlohmann::json crates = dropData["Crates"];
        for (nlohmann::json::iterator _crate = crates.begin(); _crate != crates.end(); _crate++) {
            auto crate = _crate.value();
            Crate toAdd;
            toAdd.rarityRatioId = (int)crate["RarityRatio"];
            for (nlohmann::json::iterator _itemSet = crate["ItemSets"].begin(); _itemSet != crate["ItemSets"].end(); _itemSet++) {
                toAdd.itemSets.push_back((int)_itemSet.value());
            }
            ItemManager::Crates[(int)crate["Id"]] = toAdd;
        }

        // Crate Items
        nlohmann::json items = dropData["Items"];
        int itemCount = 0;
        for (nlohmann::json::iterator _item = items.begin(); _item != items.end(); _item++) {
            auto item = _item.value();
            std::pair<int32_t, int32_t> itemSetkey = std::make_pair((int)item["ItemSet"], (int)item["Rarity"]);
            std::pair<int32_t, int32_t> itemDataKey = std::make_pair((int)item["Id"], (int)item["Type"]);

            if (ItemManager::ItemData.find(itemDataKey) == ItemManager::ItemData.end()) {
                char buff[255];
                sprintf(buff, "Unknown item with Id %d and Type %d", (int)item["Id"], (int)item["Type"]);
                throw TableException(std::string(buff));
            }

            std::map<std::pair<int32_t, int32_t>, ItemManager::Item>::iterator toAdd = ItemManager::ItemData.find(itemDataKey);

            // if item collection doesn't exist, start a new one
            if (ItemManager::CrateItems.find(itemSetkey) == ItemManager::CrateItems.end()) {
                std::vector<std::map<std::pair<int32_t, int32_t>, ItemManager::Item>::iterator> vector;
                vector.push_back(toAdd);
                ItemManager::CrateItems[itemSetkey] = vector;
            } else // else add a new element to existing collection
                ItemManager::CrateItems[itemSetkey].push_back(toAdd);

            itemCount++;
        }

        std::cout << "[INFO] Loaded " << ItemManager::Crates.size() << " Crates containing "
                  << itemCount << " items" << std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed drops.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

void TableData::loadEggs(int32_t* nextId) {
    try {
        std::ifstream inFile(settings::EGGSJSON);
        nlohmann::json eggData;

        // read file into json
        inFile >> eggData;

        // EggTypes
        nlohmann::json eggTypes = eggData["EggTypes"];
        for (nlohmann::json::iterator _eggType = eggTypes.begin(); _eggType != eggTypes.end(); _eggType++) {
            auto eggType = _eggType.value();
            EggType toAdd = {};
            toAdd.dropCrateId = (int)eggType["DropCrateId"];
            toAdd.effectId = (int)eggType["EffectId"];
            toAdd.duration = (int)eggType["Duration"];
            toAdd.regen= (int)eggType["Regen"];
            NPCManager::EggTypes[(int)eggType["Id"]] = toAdd;
        }

        // Egg instances
        auto eggs = eggData["Eggs"];
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)++;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            NPCManager::Eggs[id] = addEgg;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
        }

        std::cout << "[INFO] Loaded " <<NPCManager::Eggs.size()<<" eggs" <<std::endl;

    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed eggs.json file! Reason:" << err.what() << std::endl;
        exit(1);
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

void TableData::constructPathNPC(nlohmann::json::iterator _pathData, int32_t id) {
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

    if (id == 0)
        id = pathData["iNPCID"];

    TransportManager::NPCQueues[id] = points;
}

// load gruntwork output; if it exists
void TableData::loadGruntwork(int32_t *nextId) {
    try {
        std::ifstream inFile(settings::GRUNTWORKJSON);
        nlohmann::json gruntwork;

        // skip if there's no gruntwork to load
        if (inFile.fail())
            return;

        inFile >> gruntwork;

        // skyway paths
        auto skyway = gruntwork["skyway"];
        for (auto _route = skyway.begin(); _route != skyway.end(); _route++) {
            auto route = _route.value();
            std::vector<WarpLocation> points;

            for (auto _point = route["points"].begin(); _point != route["points"].end(); _point++) {
                auto point = _point.value();
                points.push_back(WarpLocation{point["x"], point["y"], point["z"]});
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
            NPCManager::updateNPCPosition(npc->appearanceData.iNPC_ID, npc->appearanceData.iX, npc->appearanceData.iY,
                npc->appearanceData.iZ, instanceID, npc->appearanceData.iAngle);

            RunningNPCMapNumbers[npcID] = instanceID;
        }

        // mobs
        auto mobs = gruntwork["mobs"];
        for (auto _mob = mobs.begin(); _mob != mobs.end(); _mob++) {
            auto mob = _mob.value();
            BaseNPC *npc;
            int id = (*nextId)++;
            uint64_t instanceID = mob.find("iMapNum") == mob.end() ? INSTANCE_OVERWORLD : (int)mob["iMapNum"];

            if (NPCManager::NPCData[(int)mob["iNPCType"]]["m_iTeam"] == 2) {
                npc = new Mob(mob["iX"], mob["iY"], mob["iZ"], instanceID, mob["iNPCType"],
                    NPCManager::NPCData[(int)mob["iNPCType"]], id);

                // re-enable respawning
                ((Mob*)npc)->summoned = false;

                MobManager::Mobs[npc->appearanceData.iNPC_ID] = (Mob*)npc;
            } else {
                npc = new BaseNPC(mob["iX"], mob["iY"], mob["iZ"], mob["iAngle"], instanceID, mob["iNPCType"], id);
            }

            NPCManager::NPCs[npc->appearanceData.iNPC_ID] = npc;
            TableData::RunningMobs[npc->appearanceData.iNPC_ID] = npc;
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
            MobManager::Mobs[*nextId] = (Mob*)NPCManager::NPCs[*nextId];
            NPCManager::updateNPCPosition(*nextId, leader["iX"], leader["iY"], leader["iZ"], instanceID, leader["iAngle"]);

            tmp->groupLeader = *nextId;

            (*nextId)++;

            auto followers = leader["aFollowers"];
            if (followers.size() < 5) {
                int followerCount = 0;
                for (nlohmann::json::iterator _fol = followers.begin(); _fol != followers.end(); _fol++) {
                    auto follower = _fol.value();
                    auto tdFol = NPCManager::NPCData[(int)follower["iNPCType"]];
                    Mob* tmpFol = new Mob((int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], leader["iAngle"], instanceID, follower["iNPCType"], tdFol, *nextId);

                    // re-enable respawning
                    ((Mob*)tmp)->summoned = false;

                    NPCManager::NPCs[*nextId] = tmpFol;
                    MobManager::Mobs[*nextId] = (Mob*)NPCManager::NPCs[*nextId];
                    NPCManager::updateNPCPosition(*nextId, (int)leader["iX"] + (int)follower["iOffsetX"], (int)leader["iY"] + (int)follower["iOffsetY"], leader["iZ"], instanceID, leader["iAngle"]);

                    tmpFol->offsetX = follower.find("iOffsetX") == follower.end() ? 0 : (int)follower["iOffsetX"];
                    tmpFol->offsetY = follower.find("iOffsetY") == follower.end() ? 0 : (int)follower["iOffsetY"];
                    tmpFol->groupLeader = tmp->appearanceData.iNPC_ID;
                    tmp->groupMember[followerCount++] = *nextId;

                    (*nextId)++;
                }
            }
            else {
                std::cout << "[WARN] Mob group leader with ID " << *nextId << " has too many followers (" << followers.size() << ")\n";
            }

            TableData::RunningGroups[tmp->appearanceData.iNPC_ID] = tmp; // store as running
        }

        auto eggs = gruntwork["eggs"];
        for (auto _egg = eggs.begin(); _egg != eggs.end(); _egg++) {
            auto egg = _egg.value();
            int id = (*nextId)++;
            uint64_t instanceID = egg.find("iMapNum") == egg.end() ? INSTANCE_OVERWORLD : (int)egg["iMapNum"];

            Egg* addEgg = new Egg((int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, (int)egg["iType"], id, false);
            NPCManager::NPCs[id] = addEgg;
            NPCManager::Eggs[id] = addEgg;
            NPCManager::updateNPCPosition(id, (int)egg["iX"], (int)egg["iY"], (int)egg["iZ"], instanceID, 0);
            TableData::RunningEggs[id] = addEgg;
        }


        std::cout << "[INFO] Loaded gruntwork.json" << std::endl;
    }
    catch (const std::exception& err) {
        std::cerr << "[FATAL] Malformed gruntwork.json file! Reason:" << err.what() << std::endl;
        exit(1);
    }
}

// write gruntwork output to file
void TableData::flush() {
    std::ofstream file(settings::GRUNTWORKJSON);
    nlohmann::json gruntwork;

    for (auto& pair : RunningSkywayRoutes) {
        nlohmann::json route;

        route["iRouteID"] = (int)pair.first;
        route["iMonkeySpeed"] = 1500;

        std::cout << "serializing mss route " << (int)pair.first << std::endl;
        for (WarpLocation& point : pair.second) {
            nlohmann::json tmp;

            tmp["x"] = point.x;
            tmp["y"] = point.y;
            tmp["z"] = point.z;

            route["points"].push_back(tmp);
        }

        gruntwork["skyway"].push_back(route);
    }

    for (auto& pair : RunningNPCRotations) {
        nlohmann::json rotation;

        rotation["iNPCID"] = (int)pair.first;
        rotation["iAngle"] = pair.second;

        gruntwork["rotations"].push_back(rotation);
    }

    for (auto& pair : RunningNPCMapNumbers) {
        nlohmann::json mapNumber;

        mapNumber["iNPCID"] = (int)pair.first;
        mapNumber["iMapNum"] = pair.second;

        gruntwork["instances"].push_back(mapNumber);
    }

    for (auto& pair : RunningMobs) {
        nlohmann::json mob;
        BaseNPC *npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        if (npc->npcClass == NPC_MOB) {
            Mob *m = (Mob*)npc;
            x = m->spawnX;
            y = m->spawnY;
            z = m->spawnZ;
        } else {
            x = npc->appearanceData.iX;
            y = npc->appearanceData.iY;
            z = npc->appearanceData.iZ;
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
        nlohmann::json mob;
        BaseNPC* npc = pair.second;

        if (NPCManager::NPCs.find(pair.first) == NPCManager::NPCs.end())
            continue;

        int x, y, z;
        std::vector<Mob*> followers;
        if (npc->npcClass == NPC_MOB) {
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
                if (MobManager::Mobs.find(m->groupMember[i]) == MobManager::Mobs.end()) {
                    std::cout << "[WARN] Follower with ID " << m->groupMember[i] << " not found; skipping\n";
                    continue;
                }
                followers.push_back(MobManager::Mobs[m->groupMember[i]]);
            }
        }
        else {
            x = npc->appearanceData.iX;
            y = npc->appearanceData.iY;
            z = npc->appearanceData.iZ;
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
            nlohmann::json fol;
            fol["iNPCType"] = follower->appearanceData.iNPCType;
            fol["iOffsetX"] = follower->offsetX;
            fol["iOffsetY"] = follower->offsetY;

            mob["aFollowers"].push_back(fol); // add to follower array
        }

        // it's called mobs, but really it's everything
        gruntwork["groups"].push_back(mob);
    }

    for (auto& pair : RunningEggs) {
        nlohmann::json egg;
        BaseNPC* npc = pair.second;

        if (NPCManager::Eggs.find(pair.first) == NPCManager::Eggs.end())
            continue;
        egg["iX"] = npc->appearanceData.iX;
        egg["iY"] = npc->appearanceData.iY;
        egg["iZ"] = npc->appearanceData.iZ;
        int mapnum = MAPNUM(npc->instanceID);
        if (mapnum != 0)
            egg["iMapNum"] = mapnum;
        egg["iType"] = npc->appearanceData.iNPCType;

        gruntwork["eggs"].push_back(egg);
    }

    file << gruntwork << std::endl;
}
