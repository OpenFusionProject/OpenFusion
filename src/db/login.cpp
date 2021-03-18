#include "db/internal.hpp"

#include "bcrypt/BCrypt.hpp"

void Database::findAccount(Account* account, std::string login) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT AccountID, Password, Selected, BannedUntil, BanReason
        FROM Accounts
        WHERE Login = ?
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, NULL);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        account->AccountID = sqlite3_column_int(stmt, 0);
        account->Password = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        account->Selected = sqlite3_column_int(stmt, 2);
        account->BannedUntil = sqlite3_column_int64(stmt, 3);
        account->BanReason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    }
    sqlite3_finalize(stmt);
}

int Database::addAccount(std::string login, std::string password) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO Accounts (Login, Password, AccountLevel)
        VALUES (?, ?, ?);
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, NULL);
    std::string hashedPassword = BCrypt::generateHash(password);
    sqlite3_bind_text(stmt, 2, hashedPassword.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 3, settings::ACCLEVEL);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        std::cout << "[WARN] Database: failed to add new account" << std::endl;
        return 0;
    }

    return sqlite3_last_insert_rowid(db);
}

void Database::updateSelected(int accountId, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);

    if (slot < 1 || slot > 4) {
        std::cout << "[WARN] Invalid slot number passed to updateSelected()! " << std::endl;
        return;
    }

    const char* sql = R"(
        UPDATE Accounts SET
            Selected = ?,
            LastLogin = (strftime('%s', 'now'))
        WHERE AccountID = ?;
        )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, slot);
    sqlite3_bind_int(stmt, 2, accountId);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        std::cout << "[WARN] Database fail on updateSelected(): " << sqlite3_errmsg(db) << std::endl;
}

bool Database::validateCharacter(int characterID, int userID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    // query whatever
    const char* sql = R"(
        SELECT PlayerID
        FROM Players
        WHERE PlayerID = ? AND AccountID = ?
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, characterID);
    sqlite3_bind_int(stmt, 2, userID);
    int rc = sqlite3_step(stmt);
    // if we got a row back, the character is valid
    bool result = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return result;
}

bool Database::isNameFree(std::string firstName, std::string lastName) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*)
        FROM Players
        WHERE FirstName = ? AND LastName = ?
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, firstName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, lastName.c_str(),  -1, NULL);
    int rc = sqlite3_step(stmt);

    bool result = (rc == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0);
    sqlite3_finalize(stmt);
    return result;
}

bool Database::isSlotFree(int accountId, int slotNum) {
    std::lock_guard<std::mutex> lock(dbCrit);

    if (slotNum < 1 || slotNum > 4) {
        std::cout << "[WARN] Invalid slot number passed to isSlotFree()! " << slotNum << std::endl;
        return false;
    }

    const char* sql = R"(
        SELECT COUNT(*)
        FROM Players
        WHERE AccountID = ? AND Slot = ?
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, accountId);
    sqlite3_bind_int(stmt, 2, slotNum);
    int rc = sqlite3_step(stmt);

    bool result = (rc == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0);
    sqlite3_finalize(stmt);
    return result;
}

int Database::createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        INSERT INTO Players
            (AccountID, Slot, FirstName, LastName,
             XCoordinate, YCoordinate, ZCoordinate, Angle,
             HP, NameCheck, Quests, SkywayLocationFlag, FirstUseFlag)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;
    std::string firstName = AUTOU16TOU8(save->szFirstName);
    std::string lastName =  AUTOU16TOU8(save->szLastName);

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, AccountID);
    sqlite3_bind_int(stmt, 2, save->iSlotNum);
    sqlite3_bind_text(stmt, 3, firstName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 4, lastName.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 5, settings::SPAWN_X);
    sqlite3_bind_int(stmt, 6, settings::SPAWN_Y);
    sqlite3_bind_int(stmt, 7, settings::SPAWN_Z);
    sqlite3_bind_int(stmt, 8, settings::SPAWN_ANGLE);
    sqlite3_bind_int(stmt, 9, PC_MAXHEALTH(1));

    // if FNCode isn't 0, it's a wheel name
    int nameCheck = (settings::APPROVEALLNAMES || save->iFNCode) ? 1 : 0;
    sqlite3_bind_int(stmt, 10, nameCheck);

    // blobs
    unsigned char blobBuffer[sizeof(Player::aQuestFlag)] = { 0 };
    sqlite3_bind_blob(stmt, 11, blobBuffer, sizeof(Player::aQuestFlag), NULL);
    sqlite3_bind_blob(stmt, 12, blobBuffer, sizeof(Player::aSkywayLocationFlag), NULL);
    sqlite3_bind_blob(stmt, 13, blobBuffer, sizeof(Player::iFirstUseFlag), NULL);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return 0;
    }

    int playerId = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO Appearances (PlayerID)
        VALUES (?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return 0;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return playerId;
}

bool Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character, int accountId) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        UPDATE Players
        SET AppearanceFlag = 1
        WHERE PlayerID = ? AND AccountID = ? AND AppearanceFlag = 0;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, character->PCStyle.iPC_UID);
    sqlite3_bind_int(stmt, 2, accountId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_finalize(stmt);

    sql = R"(
        UPDATE Appearances
        SET
            Body = ?,
            EyeColor = ?,
            FaceStyle = ?,
            Gender = ?,
            HairColor = ?,
            HairStyle = ?,
            Height = ?,
            SkinColor = ?
        WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, character->PCStyle.iBody);
    sqlite3_bind_int(stmt, 2, character->PCStyle.iEyeColor);
    sqlite3_bind_int(stmt, 3, character->PCStyle.iFaceStyle);
    sqlite3_bind_int(stmt, 4, character->PCStyle.iGender);
    sqlite3_bind_int(stmt, 5, character->PCStyle.iHairColor);
    sqlite3_bind_int(stmt, 6, character->PCStyle.iHairStyle);
    sqlite3_bind_int(stmt, 7, character->PCStyle.iHeight);
    sqlite3_bind_int(stmt, 8, character->PCStyle.iSkinColor);
    sqlite3_bind_int(stmt, 9, character->PCStyle.iPC_UID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO Inventory (PlayerID, Slot, ID, Type, Opt)
        VALUES (?, ?, ?, ?, 1);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    int items[3] = { character->sOn_Item.iEquipUBID, character->sOn_Item.iEquipLBID, character->sOn_Item.iEquipFootID };
    for (int i = 0; i < 3; i++) {
        sqlite3_bind_int(stmt, 1, character->PCStyle.iPC_UID);
        sqlite3_bind_int(stmt, 2, i+1);
        sqlite3_bind_int(stmt, 3, items[i]);
        sqlite3_bind_int(stmt, 4, i+1);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            return false;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

bool Database::finishTutorial(int playerID, int accountID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        UPDATE Players SET
            TutorialFlag = 1,
            Nano1 = ?,
            Quests = ?
        WHERE PlayerID = ? AND AccountID = ? AND TutorialFlag = 0;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    unsigned char questBuffer[128] = { 0 };

#ifndef ACADEMY
    // save missions nr 1 & 2; equip Buttercup
    questBuffer[0] = 3;
    sqlite3_bind_int(stmt, 1, 1);
#else
    // no, none of that
    sqlite3_bind_int(stmt, 1, 0);
#endif

    sqlite3_bind_blob(stmt, 2, questBuffer, sizeof(questBuffer), NULL);
    sqlite3_bind_int(stmt, 3, playerID);
    sqlite3_bind_int(stmt, 4, accountID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_finalize(stmt);

#ifndef ACADEMY
    // Lightning Gun
    sql = R"(
        INSERT INTO Inventory
            (PlayerID, Slot, ID, Type, Opt)
        VALUES (?, 0, 328, 0, 1);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, playerID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_finalize(stmt);

    // Nano Buttercup
    sql = R"(
        INSERT INTO Nanos
            (PlayerID, ID, Skill)
        VALUES (?, 1, 1);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, playerID);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }
#endif

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

int Database::deleteCharacter(int characterID, int userID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT Slot
        FROM Players
        WHERE AccountID = ? AND PlayerID = ?
        LIMIT 1;
        )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, userID);
    sqlite3_bind_int(stmt, 2, characterID);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    int slot = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    sql = R"(
        DELETE FROM Players
        WHERE AccountID = ? AND PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userID);
    sqlite3_bind_int(stmt, 2, characterID);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return 0;

    return slot;
}

void Database::getCharInfo(std::vector <sP_LS2CL_REP_CHAR_INFO>* result, int userID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT
            p.PlayerID, p.Slot, p.FirstName, p.LastName, p.Level, p.AppearanceFlag, p.TutorialFlag, p.PayZoneFlag,
            p.XCoordinate, p.YCoordinate, p.ZCoordinate, p.NameCheck,
            a.Body, a.EyeColor, a.FaceStyle, a.Gender, a.HairColor, a.HairStyle, a.Height, a.SkinColor
        FROM Players as p
        INNER JOIN Appearances as a ON p.PlayerID = a.PlayerID
        WHERE p.AccountID = ?;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, userID);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        sP_LS2CL_REP_CHAR_INFO toAdd = {};
        toAdd.sPC_Style.iPC_UID = sqlite3_column_int(stmt, 0);
        toAdd.iSlot = sqlite3_column_int(stmt, 1);

        // parsing const unsigned char* to char16_t
        std::string placeHolder = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        U8toU16(placeHolder, toAdd.sPC_Style.szFirstName, sizeof(toAdd.sPC_Style.szFirstName));
        placeHolder = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        U8toU16(placeHolder, toAdd.sPC_Style.szLastName, sizeof(toAdd.sPC_Style.szLastName));

        toAdd.iLevel = sqlite3_column_int(stmt, 4);
        toAdd.sPC_Style2.iAppearanceFlag = sqlite3_column_int(stmt, 5);
        toAdd.sPC_Style2.iTutorialFlag = sqlite3_column_int(stmt, 6);
        toAdd.sPC_Style2.iPayzoneFlag = sqlite3_column_int(stmt, 7);
        toAdd.iX = sqlite3_column_int(stmt, 8);
        toAdd.iY = sqlite3_column_int(stmt, 9);
        toAdd.iZ = sqlite3_column_int(stmt, 10);
        toAdd.sPC_Style.iNameCheck = sqlite3_column_int(stmt, 11);
        toAdd.sPC_Style.iBody = sqlite3_column_int(stmt, 12);
        toAdd.sPC_Style.iEyeColor = sqlite3_column_int(stmt, 13);
        toAdd.sPC_Style.iFaceStyle = sqlite3_column_int(stmt, 14);
        toAdd.sPC_Style.iGender = sqlite3_column_int(stmt, 15);
        toAdd.sPC_Style.iHairColor = sqlite3_column_int(stmt, 16);
        toAdd.sPC_Style.iHairStyle = sqlite3_column_int(stmt, 17);
        toAdd.sPC_Style.iHeight = sqlite3_column_int(stmt, 18);
        toAdd.sPC_Style.iSkinColor = sqlite3_column_int(stmt, 19);

        // request aEquip
        const char* sql2 = R"(
            SELECT Slot, Type, ID, Opt, TimeLimit
            FROM Inventory
            WHERE PlayerID = ? AND Slot < ?;
            )";
        sqlite3_stmt* stmt2;

        sqlite3_prepare_v2(db, sql2, -1, &stmt2, NULL);
        sqlite3_bind_int(stmt2, 1, toAdd.sPC_Style.iPC_UID);
        sqlite3_bind_int(stmt2, 2, AEQUIP_COUNT);

        while (sqlite3_step(stmt2) == SQLITE_ROW) {
            sItemBase* item = &toAdd.aEquip[sqlite3_column_int(stmt2, 0)];
            item->iType = sqlite3_column_int(stmt2, 1);
            item->iID = sqlite3_column_int(stmt2, 2);
            item->iOpt = sqlite3_column_int(stmt2, 3);
            item->iTimeLimit = sqlite3_column_int(stmt2, 4);
        }
        sqlite3_finalize(stmt2);

        result->push_back(toAdd);
    }
    sqlite3_finalize(stmt);
}

// NOTE: This is currently never called.
void Database::evaluateCustomName(int characterID, CustomName decision) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        UPDATE Players
        SET NameCheck = ?
        WHERE PlayerID = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, int(decision));
    sqlite3_bind_int(stmt, 2, characterID);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: Failed to update nameCheck: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
}

bool Database::changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save, int accountId) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        UPDATE Players
        SET
            FirstName = ?,
            LastName = ?,
            NameCheck = ?
        WHERE PlayerID = ? AND AccountID = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    std::string firstName = AUTOU16TOU8(save->szFirstName);
    std::string lastName = AUTOU16TOU8(save->szLastName);

    sqlite3_bind_text(stmt, 1, firstName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, lastName.c_str(), -1, NULL);
    // if FNCode isn't 0, it's a wheel name
    int nameCheck = (settings::APPROVEALLNAMES || save->iFNCode) ? 1 : 0;
    sqlite3_bind_int(stmt, 3, nameCheck);
    sqlite3_bind_int(stmt, 4, save->iPCUID);
    sqlite3_bind_int(stmt, 5, accountId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}
