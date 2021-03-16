#include "db/internal.hpp"

// Loading and saving players to/from the DB

static void removeExpiredVehicles(Player* player) {
    int32_t currentTime = getTimestamp();

    // if there are expired vehicles in bank just remove them silently
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iType == 10 && player->Bank[i].iTimeLimit < currentTime && player->Bank[i].iTimeLimit != 0) {
            memset(&player->Bank[i], 0, sizeof(sItemBase));
        }
    }

    // we want to leave only 1 expired vehicle on player to delete it with the client packet
    std::vector<sItemBase*> toRemove;

    // equipped vehicle
    if (player->Equip[8].iOpt > 0 && player->Equip[8].iTimeLimit < currentTime && player->Equip[8].iTimeLimit != 0) {
        toRemove.push_back(&player->Equip[8]);
        player->toRemoveVehicle.eIL = 0;
        player->toRemoveVehicle.iSlotNum = 8;
    }
    // inventory
    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iType == 10 && player->Inven[i].iTimeLimit < currentTime && player->Inven[i].iTimeLimit != 0) {
            toRemove.push_back(&player->Inven[i]);
            player->toRemoveVehicle.eIL = 1;
            player->toRemoveVehicle.iSlotNum = i;
        }
    }

    // delete all but one vehicles, leave last one for ceremonial deletion
    for (int i = 0; i < (int)toRemove.size()-1; i++) {
        memset(toRemove[i], 0, sizeof(sItemBase));
    }
}

void Database::getPlayer(Player* plr, int id) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT
            p.AccountID, p.Slot, p.FirstName, p.LastName,
            p.Level, p.Nano1, p.Nano2, p.Nano3,
            p.AppearanceFlag, p.TutorialFlag, p.PayZoneFlag,
            p.XCoordinate, p.YCoordinate, p.ZCoordinate, p.NameCheck,
            p.Angle, p.HP, acc.AccountLevel, p.FusionMatter, p.Taros, p.Quests,
            p.BatteryW, p.BatteryN, p.Mentor, p.WarpLocationFlag,
            p.SkywayLocationFlag, p.CurrentMissionID, p.FirstUseFlag,
            a.Body, a.EyeColor, a.FaceStyle, a.Gender, a.HairColor, a.HairStyle, a.Height, a.SkinColor
        FROM Players as p
        INNER JOIN Appearances as a ON p.PlayerID = a.PlayerID
        INNER JOIN Accounts as acc ON p.AccountID = acc.AccountID
        WHERE p.PlayerID = ?;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "[WARN] Database: Failed to load character [" << id << "]: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    plr->iID = id;
    plr->PCStyle.iPC_UID = id;

    plr->accountId = sqlite3_column_int(stmt, 0);
    plr->slot = sqlite3_column_int(stmt, 1);

    // parsing const unsigned char* to char16_t
    std::string placeHolder = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    U8toU16(placeHolder, plr->PCStyle.szFirstName, sizeof(plr->PCStyle.szFirstName));
    placeHolder = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    U8toU16(placeHolder, plr->PCStyle.szLastName, sizeof(plr->PCStyle.szLastName));

    plr->level = sqlite3_column_int(stmt, 4);
    plr->equippedNanos[0] = sqlite3_column_int(stmt, 5);
    plr->equippedNanos[1] = sqlite3_column_int(stmt, 6);
    plr->equippedNanos[2] = sqlite3_column_int(stmt, 7);

    plr->PCStyle2.iAppearanceFlag = sqlite3_column_int(stmt, 8);
    plr->PCStyle2.iTutorialFlag = sqlite3_column_int(stmt, 9);
    plr->PCStyle2.iPayzoneFlag = sqlite3_column_int(stmt, 10);

    plr->x = sqlite3_column_int(stmt, 11);
    plr->y = sqlite3_column_int(stmt, 12);
    plr->z = sqlite3_column_int(stmt, 13);
    plr->PCStyle.iNameCheck = sqlite3_column_int(stmt, 14);

    plr->angle = sqlite3_column_int(stmt, 15);
    plr->HP = sqlite3_column_int(stmt, 16);
    plr->accountLevel = sqlite3_column_int(stmt, 17);
    plr->fusionmatter = sqlite3_column_int(stmt, 18);
    plr->money = sqlite3_column_int(stmt, 19);

    memcpy(plr->aQuestFlag, sqlite3_column_blob(stmt, 20), sizeof(plr->aQuestFlag));

    plr->batteryW = sqlite3_column_int(stmt, 21);
    plr->batteryN = sqlite3_column_int(stmt, 22);
    plr->mentor = sqlite3_column_int(stmt, 23);
    plr->iWarpLocationFlag = sqlite3_column_int(stmt, 24);

    memcpy(plr->aSkywayLocationFlag, sqlite3_column_blob(stmt, 25), sizeof(plr->aSkywayLocationFlag));

    plr->CurrentMissionID = sqlite3_column_int(stmt, 26);

    memcpy(plr->iFirstUseFlag, sqlite3_column_blob(stmt, 27), sizeof(plr->iFirstUseFlag));

    plr->PCStyle.iBody = sqlite3_column_int(stmt, 28);
    plr->PCStyle.iEyeColor = sqlite3_column_int(stmt, 29);
    plr->PCStyle.iFaceStyle = sqlite3_column_int(stmt, 30);
    plr->PCStyle.iGender = sqlite3_column_int(stmt, 31);
    plr->PCStyle.iHairColor = sqlite3_column_int(stmt, 32);
    plr->PCStyle.iHairStyle = sqlite3_column_int(stmt, 33);
    plr->PCStyle.iHeight = sqlite3_column_int(stmt, 34);
    plr->PCStyle.iSkinColor = sqlite3_column_int(stmt, 35);

    sqlite3_finalize(stmt);

    // get inventory
    sql = R"(
        SELECT Slot, Type, ID, Opt, TimeLimit
        FROM Inventory
        WHERE PlayerID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt, 0);

        // for extra safety
        if (slot < 0 || slot > AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT) {
            std::cout << "[WARN] Database: Invalid item slot in db?! " << std::endl;
            continue;
        }

        sItemBase* item;
        if (slot < AEQUIP_COUNT) {
            // equipment
            item = &plr->Equip[slot];
        } else if (slot < (AEQUIP_COUNT + AINVEN_COUNT)) {
            // inventory
            item = &plr->Inven[slot - AEQUIP_COUNT];
        } else {
            // bank
            item = &plr->Bank[slot - AEQUIP_COUNT - AINVEN_COUNT];
        }

        item->iType = sqlite3_column_int(stmt, 1);
        item->iID = sqlite3_column_int(stmt, 2);
        item->iOpt = sqlite3_column_int(stmt, 3);
        item->iTimeLimit = sqlite3_column_int(stmt, 4);
    }

    sqlite3_finalize(stmt);

    removeExpiredVehicles(plr);

    // get quest inventory
    sql = R"(
        SELECT Slot, ID, Opt
        FROM QuestItems
        WHERE PlayerID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt, 0);

        // for extra safety
        if (slot < 0)
            continue;

        sItemBase* item = &plr->QInven[slot];
        item->iType = 8;
        item->iID = sqlite3_column_int(stmt, 1);
        item->iOpt = sqlite3_column_int(stmt, 2);
    }

    sqlite3_finalize(stmt);

    // get nanos
    sql = R"(
        SELECT ID, Skill, Stamina
        FROM Nanos
        WHERE PlayerID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);

        // for extra safety
        if (id < 0 || id > NANO_COUNT)
            continue;

        sNano* nano = &plr->Nanos[id];
        nano->iID = id;
        nano->iSkillID = sqlite3_column_int(stmt, 1);
        nano->iStamina = sqlite3_column_int(stmt, 2);
    }

    sqlite3_finalize(stmt);

    // get active quests
    sql = R"(
        SELECT
            TaskID,
            RemainingNPCCount1,
            RemainingNPCCount2,
            RemainingNPCCount3
        FROM RunningQuests
        WHERE PlayerID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);

    std::set<int> tasksSet; // used to prevent duplicate tasks from loading in
    for (int i = 0; sqlite3_step(stmt) == SQLITE_ROW && i < ACTIVE_MISSION_COUNT; i++) {

        int taskID = sqlite3_column_int(stmt, 0);
        if (tasksSet.find(taskID) != tasksSet.end())
            continue;

        plr->tasks[i] = taskID;
        tasksSet.insert(taskID);
        plr->RemainingNPCCount[i][0] = sqlite3_column_int(stmt, 1);
        plr->RemainingNPCCount[i][1] = sqlite3_column_int(stmt, 2);
        plr->RemainingNPCCount[i][2] = sqlite3_column_int(stmt, 3);
    }

    sqlite3_finalize(stmt);

    // get buddies
    sql = R"(
        SELECT PlayerAID, PlayerBID
        FROM Buddyships
        WHERE PlayerAID = ? OR PlayerBID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, id);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < 50) {
        int PlayerAId = sqlite3_column_int(stmt, 0);
        int PlayerBId = sqlite3_column_int(stmt, 1);

        plr->buddyIDs[i] = id == PlayerAId ? PlayerBId : PlayerAId;
        plr->isBuddyBlocked[i] = false;
        i++;
    }

    sqlite3_finalize(stmt);

    // get blocked players
    sql = R"(
        SELECT BlockedPlayerID FROM Blocks
        WHERE PlayerID = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);

    // i retains its value from after the loop over Buddyships
    while (sqlite3_step(stmt) == SQLITE_ROW && i < 50) {
        plr->buddyIDs[i] = sqlite3_column_int(stmt, 0);
        plr->isBuddyBlocked[i] = true;
        i++;
    }

    sqlite3_finalize(stmt);
}

void Database::updatePlayer(Player *player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        UPDATE Players
        SET
            Level = ? , Nano1 = ?, Nano2 = ?, Nano3 = ?,
            XCoordinate = ?, YCoordinate = ?, ZCoordinate = ?,
            Angle = ?, HP = ?, FusionMatter = ?, Taros = ?, Quests = ?,
            BatteryW = ?, BatteryN = ?, WarplocationFlag = ?,
            SkywayLocationFlag = ?, CurrentMissionID = ?,
            PayZoneFlag = ?, FirstUseFlag = ?, Mentor = ?
        WHERE PlayerID = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->level);
    sqlite3_bind_int(stmt, 2, player->equippedNanos[0]);
    sqlite3_bind_int(stmt, 3, player->equippedNanos[1]);
    sqlite3_bind_int(stmt, 4, player->equippedNanos[2]);

    if (player->instanceID == 0 && !player->onMonkey) {
        sqlite3_bind_int(stmt, 5, player->x);
        sqlite3_bind_int(stmt, 6, player->y);
        sqlite3_bind_int(stmt, 7, player->z);
        sqlite3_bind_int(stmt, 8, player->angle);
    }
    else {
        sqlite3_bind_int(stmt, 5, player->lastX);
        sqlite3_bind_int(stmt, 6, player->lastY);
        sqlite3_bind_int(stmt, 7, player->lastZ);
        sqlite3_bind_int(stmt, 8, player->lastAngle);
    }

    sqlite3_bind_int(stmt, 9, player->HP);
    sqlite3_bind_int(stmt, 10, player->fusionmatter);
    sqlite3_bind_int(stmt, 11, player->money);
    sqlite3_bind_blob(stmt, 12, player->aQuestFlag, sizeof(player->aQuestFlag), NULL);
    sqlite3_bind_int(stmt, 13, player->batteryW);
    sqlite3_bind_int(stmt, 14, player->batteryN);
    sqlite3_bind_int(stmt, 15, player->iWarpLocationFlag);
    sqlite3_bind_blob(stmt, 16, player->aSkywayLocationFlag, sizeof(player->aSkywayLocationFlag), NULL);
    sqlite3_bind_int(stmt, 17, player->CurrentMissionID);
    sqlite3_bind_int(stmt, 18, player->PCStyle2.iPayzoneFlag);
    sqlite3_bind_blob(stmt, 19, player->iFirstUseFlag, sizeof(player->iFirstUseFlag), NULL);
    sqlite3_bind_int(stmt, 20, player->mentor);
    sqlite3_bind_int(stmt, 21, player->iID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return;
    }

    sqlite3_finalize(stmt);

    // update inventory
    sql = R"(
        DELETE FROM Inventory WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO Inventory
            (PlayerID, Slot, Type, Opt, ID, Timelimit)
        VALUES (?, ?, ?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    for (int i = 0; i < AEQUIP_COUNT; i++) {
        if (player->Equip[i].iID == 0)
            continue;

        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_int(stmt, 3, player->Equip[i].iType);
        sqlite3_bind_int(stmt, 4, player->Equip[i].iOpt);
        sqlite3_bind_int(stmt, 5, player->Equip[i].iID);
        sqlite3_bind_int(stmt, 6, player->Equip[i].iTimeLimit);

        rc = sqlite3_step(stmt);

        if (rc != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iID == 0)
            continue;

        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i + AEQUIP_COUNT);
        sqlite3_bind_int(stmt, 3, player->Inven[i].iType);
        sqlite3_bind_int(stmt, 4, player->Inven[i].iOpt);
        sqlite3_bind_int(stmt, 5, player->Inven[i].iID);
        sqlite3_bind_int(stmt, 6, player->Inven[i].iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iID == 0)
            continue;

        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i + AEQUIP_COUNT + AINVEN_COUNT);
        sqlite3_bind_int(stmt, 3, player->Bank[i].iType);
        sqlite3_bind_int(stmt, 4, player->Bank[i].iOpt);
        sqlite3_bind_int(stmt, 5, player->Bank[i].iID);
        sqlite3_bind_int(stmt, 6, player->Bank[i].iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    // Update Quest Inventory
    sql = R"(
        DELETE FROM QuestItems WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO QuestItems (PlayerID, Slot, Opt, ID)
        VALUES (?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    for (int i = 0; i < AQINVEN_COUNT; i++) {
        if (player->QInven[i].iID == 0)
            continue;

        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_int(stmt, 3, player->QInven[i].iOpt);
        sqlite3_bind_int(stmt, 4, player->QInven[i].iID);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    // Update Nanos
    sql = R"(
        DELETE FROM Nanos WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO Nanos (PlayerID, ID, SKill, Stamina)
        VALUES (?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    for (int i = 0; i < NANO_COUNT; i++) {
        if (player->Nanos[i].iID == 0)
            continue;

        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, player->Nanos[i].iID);
        sqlite3_bind_int(stmt, 3, player->Nanos[i].iSkillID);
        sqlite3_bind_int(stmt, 4, player->Nanos[i].iStamina);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    // Update Running Quests
    sql = R"(
        DELETE FROM RunningQuests WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO RunningQuests
            (PlayerID, TaskID, RemainingNPCCount1, RemainingNPCCount2, RemainingNPCCount3)
        VALUES (?, ?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (player->tasks[i] == 0)
            continue;
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, player->tasks[i]);
        sqlite3_bind_int(stmt, 3, player->RemainingNPCCount[i][0]);
        sqlite3_bind_int(stmt, 4, player->RemainingNPCCount[i][1]);
        sqlite3_bind_int(stmt, 5, player->RemainingNPCCount[i][2]);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
}
