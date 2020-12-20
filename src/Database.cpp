#include "Database.hpp"
#include "Database.hpp"
#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "settings.hpp"
#include "Player.hpp"
#include "CNStructs.hpp"
#include "MissionManager.hpp"

#include "contrib/JSON.hpp"
#include "contrib/bcrypt/BCrypt.hpp"

#include <string>
#include <sqlite3.h>

#include <iostream>
#include <fstream>
#include <sstream>

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

std::mutex dbCrit;
sqlite3* db;

void Database::open() {
    int rc = sqlite3_open(settings::DBPATH.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cout << "[FATAL] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        exit(1);
    }

    // foreign keys in sqlite are off by default; enable them
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    // just in case a DB operation collides with an external manual modification
    sqlite3_busy_timeout(db, 2000);

    checkMetaTable();
    createTables();

    std::cout << "[INFO] Database in operation ";
    int accounts = getTableSize("Accounts");
    int players = getTableSize("Players");
    std::string message = "";
    if (accounts > 0) {
        message += ": Found " + std::to_string(accounts) + " Account";
        if (accounts > 1)
            message += "s";
    }
    if (players > 0) {
        message += " and " + std::to_string(players) + " Player Character";
        if (players > 1)
            message += "s";
    }
    std::cout << message << std::endl;
}

void Database::close() {
    sqlite3_close(db);
}

void Database::checkMetaTable() {
    // first check if meta table exists
    const char* sql = R"(
        SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='Meta';
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cout << "[FATAL] Failed to check meta table" << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        exit(1);
    }

    int count = sqlite3_column_int(stmt, 0);
    if (count == 0) {
        sqlite3_finalize(stmt);
        // check if there's other non-internal tables first
        sql = R"(
            SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';
            )";
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (sqlite3_step(stmt) != SQLITE_ROW || sqlite3_column_int(stmt, 0) != 0) {
            sqlite3_finalize(stmt);
            std::cout << "[FATAL] Existing DB is outdated" << std::endl;
            exit(1);
        }

        // create meta table
        sqlite3_finalize(stmt);
        return createMetaTable();
    }

    sqlite3_finalize(stmt);

    // check protocol version
    sql = R"(
        SELECT Value FROM Meta WHERE Key = 'ProtocolVersion';
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cout << "[FATAL] Failed to check DB Protocol Version: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        exit(1);
    }

    if (sqlite3_column_int(stmt, 0) != PROTOCOL_VERSION) {
        sqlite3_finalize(stmt);
        std::cout << "[FATAL] DB Protocol Version doesn't match Server Build" << std::endl;
        exit(1);
    }

    sqlite3_finalize(stmt);

    sql = R"(
        SELECT Value FROM Meta WHERE Key = 'DatabaseVersion';
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cout << "[FATAL] Failed to check DB Version: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        exit(1);
    }

    int dbVersion = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (dbVersion > DATABASE_VERSION) {
        std::cout << "[FATAL] Server Build is incompatible with DB Version" << std::endl;
        exit(1);
    } else if (dbVersion < DATABASE_VERSION) {
        // we're gonna migrate; back up the DB
        std::cout << "[INFO] Backing up database" << std::endl;
        // copy db file over using binary streams
        std::ifstream  src(settings::DBPATH, std::ios::binary);
        std::ofstream  dst(settings::DBPATH + ".old." + std::to_string(dbVersion), std::ios::binary);
        dst << src.rdbuf();
        src.close();
        dst.close();
    }

    while (dbVersion != DATABASE_VERSION) {
        // db migrations
        std::cout << "[INFO] Migrating Database to Version " << dbVersion + 1 << std::endl;

        std::string path = "sql/migration" + std::to_string(dbVersion) + ".sql";
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "[FATAL] Failed to migrate database: Couldn't open migration file" << std::endl;
            exit(1);
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        std::string sql = stream.str();
        int rc = sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);

        if (rc != SQLITE_OK) {
            std::cout << "[FATAL] Failed to migrate database: " << sqlite3_errmsg(db) << std::endl;
            exit(1);
        }

        dbVersion++;
        std::cout << "[INFO] Successful Database Migration to Version " << dbVersion << std::endl;
    }    
}

void Database::createMetaTable() {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        CREATE TABLE Meta(
            Key TEXT NOT NULL UNIQUE,
            Value INTEGER NOT NULL
        );
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[FATAL] Failed to create meta table: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        exit(1);
    }
    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO Meta (Key, Value)
        VALUES (?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, "ProtocolVersion", -1, NULL);
    sqlite3_bind_int(stmt, 2, PROTOCOL_VERSION);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[FATAL] Failed to create meta table: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        exit(1);
    }

    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, "DatabaseVersion", -1, NULL);
    sqlite3_bind_int(stmt, 2, DATABASE_VERSION);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        std::cout << "[FATAL] Failed to create meta table: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        exit(1);
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    std::cout << "[INFO] Created new meta table" << std::endl;
}

void Database::createTables() {
    std::ifstream file("sql/tables.sql");
    if (!file.is_open()) {
        std::cout << "[FATAL] Failed to open database scheme" << std::endl;
        exit(1);
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    std::string read = stream.str();
    const char* sql = read.c_str();

    char* errMsg = 0;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
    if (rc != SQLITE_OK) {
        std::cout << "[FATAL] Database failed to create tables: " << errMsg << std::endl;
        exit(1);
    }
}

int Database::getTableSize(std::string tableName) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = "SELECT COUNT(*) FROM ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, NULL);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

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

void Database::banAccount(int accountId, int days) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        UPDATE Accounts SET
            BannedSince = (strftime('%s', 'now')),
            BannedUntil = (strftime('%s', 'now')) + ?
        WHERE AccountID = ?;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, days * 86400); // convert days to seconds
    sqlite3_bind_int(stmt, 2, accountId);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: failed to ban player: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
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
             XCoordinates, YCoordinates, ZCoordinates, Angle,
             HP, NameCheck, Quests, SkywayLocationFlag, FirstUseFlag)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;
    std::string firstName = U16toU8(save->szFirstName);
    std::string lastName =  U16toU8(save->szLastName);

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
            Nano1 = 1,
            Quests = ?
        WHERE PlayerID = ? AND AccountID = ? AND TutorialFlag = 0;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    // save missions nr 1 & 2
    unsigned char questBuffer[128] = { 0 };
    questBuffer[0] = 3;
    sqlite3_bind_blob(stmt, 1, questBuffer, sizeof(questBuffer), NULL);
    sqlite3_bind_int(stmt, 2, playerID);
    sqlite3_bind_int(stmt, 3, accountID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_finalize(stmt);

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
            p.XCoordinates, p.YCoordinates, p.ZCoordinates, p.NameCheck,
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

    std::string firstName = U16toU8(save->szFirstName);
    std::string lastName = U16toU8(save->szLastName);

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

void Database::getPlayer(Player* plr, int id) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT
            p.AccountID, p.Slot, p.FirstName, p.LastName,
            p.Level, p.Nano1, p.Nano2, p.Nano3,
            p.AppearanceFlag, p.TutorialFlag, p.PayZoneFlag,
            p.XCoordinates, p.YCoordinates, p.ZCoordinates, p.NameCheck,
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
        if (slot > AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT) {
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

    Database::removeExpiredVehicles(plr);

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
        if (id > SIZEOF_NANO_BANK_SLOT)
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

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < ACTIVE_MISSION_COUNT) {
        plr->tasks[i] = sqlite3_column_int(stmt, 0);
        plr->RemainingNPCCount[i][0] = sqlite3_column_int(stmt, 1);
        plr->RemainingNPCCount[i][1] = sqlite3_column_int(stmt, 2);
        plr->RemainingNPCCount[i][2] = sqlite3_column_int(stmt, 3);
        i++;
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

    i = 0;
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
            XCoordinates = ?, YCoordinates = ?, ZCoordinates = ?,
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

    for (int i = 0; i < SIZEOF_NANO_BANK_SLOT; i++) {
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

void Database::removeExpiredVehicles(Player* player) {
    int32_t currentTime = getTimestamp();

    // if there are expired vehicles in bank just remove them silently
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iType == 10 && player->Bank[i].iTimeLimit < currentTime) {
            memset(&player->Bank[i], 0, sizeof(sItemBase));
        }
    }

    // we want to leave only 1 expired vehicle on player to delete it with the client packet
    std::vector<sItemBase*> toRemove;

    // equipped vehicle
    if (player->Equip[8].iOpt > 0 && player->Equip[8].iTimeLimit < currentTime) {
        toRemove.push_back(&player->Equip[8]);
        player->toRemoveVehicle.eIL = 0;
        player->toRemoveVehicle.iSlotNum = 8;
    }
    // inventory
    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iType == 10 && player->Inven[i].iTimeLimit < currentTime) {
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

// buddies
// returns num of buddies + blocked players
int Database::getNumBuddies(Player* player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*)
        FROM Buddyships
        WHERE PlayerAID = ? OR PlayerBID = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_bind_int(stmt, 2, player->iID);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    sql = R"(
        SELECT COUNT(*)
        FROM Blocks
        WHERE PlayerID = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);
    result += sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    // again, for peace of mind
    return result > 50 ? 50 : result;
}

void Database::addBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO Buddyships (PlayerAID, PlayerBID)
        VALUES (?, ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerA);
    sqlite3_bind_int(stmt, 2, playerB);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to add buddyship: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
}

void Database::removeBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        DELETE FROM Buddyships
        WHERE (PlayerAID = ? AND PlayerBID = ?) OR (PlayerAID = ? AND PlayerBID = ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerA);
    sqlite3_bind_int(stmt, 2, playerB);
    sqlite3_bind_int(stmt, 3, playerB);
    sqlite3_bind_int(stmt, 4, playerA);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// blocking
void Database::addBlock(int playerId, int blockedPlayerId) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO Blocks (PlayerID, BlockedPlayerID)
        VALUES (?, ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);
    sqlite3_bind_int(stmt, 2, blockedPlayerId);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to block player: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
}

void Database::removeBlock(int playerId, int blockedPlayerId) {
    const char* sql = R"(
        DELETE FROM Blocks
        WHERE PlayerID = ? AND BlockedPlayerID = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);
    sqlite3_bind_int(stmt, 2, blockedPlayerId);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// email
int Database::getUnreadEmailCount(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*) FROM EmailData
        WHERE PlayerID = ? AND ReadFlag = 0;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_step(stmt);
    int ret = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    return ret;
}

std::vector<Database::EmailData> Database::getEmails(int playerID, int page) {
    std::lock_guard<std::mutex> lock(dbCrit);

    std::vector<Database::EmailData> emails;

    const char* sql = R"(
        SELECT
            MsgIndex, ItemFlag, ReadFlag, SenderID,
            SenderFirstName, SenderLastName, SubjectLine,
            MsgBody, Taros, SendTime, DeleteTime
        FROM EmailData
        WHERE PlayerID = ?
        ORDER BY MsgIndex DESC
        LIMIT 5
        OFFSET ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    int offset = 5 * page - 5;
    sqlite3_bind_int(stmt, 2, offset);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Database::EmailData toAdd;
        toAdd.PlayerId = playerID;
        toAdd.MsgIndex = sqlite3_column_int(stmt, 0);
        toAdd.ItemFlag = sqlite3_column_int(stmt, 1);
        toAdd.ReadFlag = sqlite3_column_int(stmt, 2);
        toAdd.SenderId = sqlite3_column_int(stmt, 3);
        toAdd.SenderFirstName = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        toAdd.SenderLastName = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        toAdd.SubjectLine = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
        toAdd.MsgBody = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));
        toAdd.Taros = sqlite3_column_int(stmt, 8);
        toAdd.SendTime = sqlite3_column_int64(stmt, 9);
        toAdd.DeleteTime = sqlite3_column_int64(stmt, 10);

        emails.push_back(toAdd);
    }
    sqlite3_finalize(stmt);

    return emails;
}

Database::EmailData Database::getEmail(int playerID, int index) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT
            ItemFlag, ReadFlag, SenderID, SenderFirstName,
            SenderLastName, SubjectLine, MsgBody,
            Taros, SendTime, DeleteTime
        FROM EmailData
        WHERE PlayerID = ? AND MsgIndex = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_bind_int(stmt, 2, index);

    Database::EmailData result;
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cout << "[WARN] Database: Email not found!" << std::endl;
        sqlite3_finalize(stmt);
        return result;
    }

    result.PlayerId = playerID;
    result.MsgIndex = index;
    result.ItemFlag = sqlite3_column_int(stmt, 0);
    result.ReadFlag = sqlite3_column_int(stmt, 1);
    result.SenderId = sqlite3_column_int(stmt, 2);
    result.SenderFirstName = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
    result.SenderLastName = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
    result.SubjectLine = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    result.MsgBody = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
    result.Taros = sqlite3_column_int(stmt, 7);
    result.SendTime = sqlite3_column_int64(stmt, 8);
    result.DeleteTime = sqlite3_column_int64(stmt, 9);

    sqlite3_finalize(stmt);
    return result;
}

sItemBase* Database::getEmailAttachments(int playerID, int index) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sItemBase* items = new sItemBase[4];
    for (int i = 0; i < 4; i++)
        items[i] = { 0, 0, 0, 0 };

    const char* sql = R"(
        SELECT Slot, ID, Type, Opt, TimeLimit
        FROM EmailItems
        WHERE PlayerID = ? AND MsgIndex = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_bind_int(stmt, 2, index);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt, 0) - 1;
        if (slot < 0 || slot > 3) {
            std::cout << "[WARN] Email item has invalid slot number ?!" << std::endl;
            continue;
        }

        items[slot].iID = sqlite3_column_int(stmt, 1);
        items[slot].iType = sqlite3_column_int(stmt, 2);
        items[slot].iOpt = sqlite3_column_int(stmt, 3);
        items[slot].iTimeLimit = sqlite3_column_int(stmt, 4);
    }

    sqlite3_finalize(stmt);
    return items;
}

void Database::updateEmailContent(EmailData* data) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*)
        FROM EmailItems
        WHERE PlayerID = ? AND MsgIndex = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_step(stmt);
    int attachmentsCount = sqlite3_column_int(stmt, 0);

    data->ItemFlag = (data->Taros > 0 || attachmentsCount > 0) ? 1 : 0; // set attachment flag dynamically

    sqlite3_finalize(stmt);

    sql = R"(
        UPDATE EmailData
        SET
            PlayerID = ?,
            MsgIndex = ?,
            ReadFlag = ?,
            ItemFlag = ?,
            SenderID = ?,
            SenderFirstName = ?,
            SenderLastName = ?,
            SubjectLine = ?,
            MsgBody = ?,
            Taros = ?,
            SendTime = ?,
            DeleteTime = ?
        WHERE PlayerID = ? AND MsgIndex = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_bind_int(stmt, 3, data->ReadFlag);
    sqlite3_bind_int(stmt, 4, data->ItemFlag);
    sqlite3_bind_int(stmt, 5, data->SenderId);
    sqlite3_bind_text(stmt, 6, data->SenderFirstName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 7, data->SenderLastName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 8, data->SubjectLine.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 9, data->MsgBody.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 10, data->Taros);
    sqlite3_bind_int64(stmt, 11, data->SendTime);
    sqlite3_bind_int64(stmt, 12, data->DeleteTime);
    sqlite3_bind_int(stmt, 13, data->PlayerId);
    sqlite3_bind_int(stmt, 14, data->MsgIndex);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to update email: " << sqlite3_errmsg(db) << std::endl;

    sqlite3_finalize(stmt);
}

void Database::deleteEmailAttachments(int playerID, int index, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_stmt* stmt;

    std::string sql(R"(
        DELETE FROM EmailItems
        WHERE PlayerID = ? AND MsgIndex = ?;
        )");

    if (slot != -1)
        sql += " AND \"Slot\" = ? ";
    sql += ";";

    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_bind_int(stmt, 2, index);
    if (slot != -1)
        sqlite3_bind_int(stmt, 3, slot);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: Failed to delete email attachments: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
}

void Database::deleteEmails(int playerID, int64_t* indices) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sqlite3_stmt* stmt;

    const char* sql = R"(
        DELETE FROM EmailData
        WHERE PlayerID = ? AND MsgIndex = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    for (int i = 0; i < 5; i++) {
        sqlite3_bind_int(stmt, 1, playerID);
        sqlite3_bind_int64(stmt, 2, indices[i]);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to delete an email: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
}

int Database::getNextEmailIndex(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT MsgIndex
        FROM EmailData
        WHERE PlayerID = ?
        ORDER BY MsgIndex DESC
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_step(stmt);
    int index = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return (index > 0 ? index + 1 : 1);
}

bool Database::sendEmail(EmailData* data, std::vector<sItemBase> attachments) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    const char* sql = R"(
        INSERT INTO EmailData
            (PlayerID, MsgIndex, ReadFlag, ItemFlag,
            SenderID, SenderFirstName, SenderLastName,
            SubjectLine, MsgBody, Taros, SendTime, DeleteTime)
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_bind_int(stmt, 3, data->ReadFlag);
    sqlite3_bind_int(stmt, 4, data->ItemFlag);
    sqlite3_bind_int(stmt, 5, data->SenderId);
    sqlite3_bind_text(stmt, 6, data->SenderFirstName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 7, data->SenderLastName.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 8, data->SubjectLine.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 9, data->MsgBody.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 10, data->Taros);
    sqlite3_bind_int64(stmt, 11, data->SendTime);
    sqlite3_bind_int64(stmt, 12, data->DeleteTime);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: Failed to send email: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);

    sql = R"(
        INSERT INTO EmailItems
            (PlayerID, MsgIndex, Slot, ID, Type, Opt, TimeLimit)
        VALUES (?, ?, ?, ?, ?, ?, ?);
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    // send attachments
    int slot = 1;
    for (sItemBase item : attachments) {
        sqlite3_bind_int(stmt, 1, data->PlayerId);
        sqlite3_bind_int(stmt, 2, data->MsgIndex);
        sqlite3_bind_int(stmt, 3, slot++);
        sqlite3_bind_int(stmt, 4, item.iID);
        sqlite3_bind_int(stmt, 5, item.iType);
        sqlite3_bind_int(stmt, 6, item.iOpt);
        sqlite3_bind_int(stmt, 7, item.iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to send email: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}

