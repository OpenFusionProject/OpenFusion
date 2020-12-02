#include "Database.hpp"
#include "Database.hpp"
#include "contrib/bcrypt/BCrypt.hpp"
#include "CNProtocol.hpp"
#include <string>
#include "contrib/JSON.hpp"
#include "contrib/sqlite/sqlite3.h"
#include "CNStructs.hpp"
#include "settings.hpp"
#include "Player.hpp"
#include "CNStructs.hpp"
#include "MissionManager.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

std::mutex dbCrit;
sqlite3* db;

#pragma region LoginServer

void Database::open() {
    
    int rc = sqlite3_open(settings::DBPATH.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cout << "[FATAL] Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        terminate(0);
    }

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

void Database::createTables() {

    char* errMsg = 0;
    char* sql;

    sql = R"(
        CREATE TABLE IF NOT EXISTS "Accounts" (
        "AccountID"	INTEGER NOT NULL,
        "Login"	    TEXT    NOT NULL UNIQUE,
        "Password"	TEXT    NOT NULL,
        "Selected"	INTEGER  DEFAULT 1 NOT NULL,
        "Created"	INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
        "LastLogin"	INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
        PRIMARY KEY("AccountID" AUTOINCREMENT)
        );

         CREATE TABLE IF NOT EXISTS "Players" (
        "PlayerID"	INTEGER NOT NULL,
        "AccountID"	INTEGER NOT NULL,
        "Slot"	    INTEGER NOT NULL,
        "Firstname"	TEXT    NOT NULL COLLATE NOCASE,
        "LastName"	TEXT    NOT NULL COLLATE NOCASE,
        "Created"	INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
        "LastLogin"	INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
        "Level"	    INTEGER DEFAULT 1 NOT NULL,
        "Nano1"	    INTEGER DEFAULT 0 NOT NULL,
        "Nano2"	    INTEGER DEFAULT 0 NOT NULL,
        "Nano3"	    INTEGER DEFAULT 0 NOT NULL,
        "AppearanceFlag" INTEGER DEFAULT 0 NOT NULL,
        "TutorialFlag"	 INTEGER DEFAULT 0 NOT NULL,
        "PayZoneFlag"	 INTEGER DEFAULT 0 NOT NULL,
        "XCoordinates"	 INTEGER NOT NULL,
        "YCoordinates"	 INTEGER NOT NULL,
        "ZCoordinates"	 INTEGER NOT NULL,
        "Angle"	    INTEGER NOT NULL,
        "HP"	         INTEGER NOT NULL,
        "NameCheck"	     INTEGER NOT NULL,     
        "AccountLevel"	INTEGER NOT NULL,
        "FusionMatter"	INTEGER DEFAULT 0 NOT NULL,
        "Taros"	        INTEGER DEFAULT 0 NOT NULL,
        "Quests"	BLOB    NOT NULL,
        "BatteryW"	INTEGER DEFAULT 0 NOT NULL,
        "BatteryN"	INTEGER DEFAULT 0 NOT NULL,
        "Mentor"	INTEGER DEFAULT 5 NOT NULL,
        "WarpLocationFlag"	    INTEGER DEFAULT 0 NOT NULL,
        "SkywayLocationFlag1"	INTEGER DEFAULT 0 NOT NULL,
        "SkywayLocationFlag2"	INTEGER DEFAULT 0 NOT NULL,
        "CurrentMissionID"	    INTEGER DEFAULT 0 NOT NULL,
        PRIMARY KEY("PlayerID" AUTOINCREMENT),
        FOREIGN KEY("AccountID") REFERENCES "Accounts"("AccountID") ON DELETE CASCADE,
        UNIQUE ("AccountID", "Slot"),
        UNIQUE ("Firstname", "LastName")
        );

        CREATE TABLE IF NOT EXISTS "Appearances" (
        "PlayerID"	INTEGER UNIQUE NOT NULL,
        "Body"	    INTEGER DEFAULT 0 NOT NULL,
        "EyeColor"	INTEGER DEFAULT 1 NOT NULL,
        "FaceStyle"	INTEGER DEFAULT 1 NOT NULL,
        "Gender"	INTEGER DEFAULT 1 NOT NULL,
        "HairColor"	INTEGER DEFAULT 1 NOT NULL,
        "HairStyle"	INTEGER DEFAULT 1 NOT NULL,
        "Height"	INTEGER DEFAULT 0 NOT NULL,
        "SkinColor"	INTEGER DEFAULT 1 NOT NULL,
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS "Inventory" (
	    "PlayerId"	INTEGER NOT NULL,
	    "Slot"	    INTEGER NOT NULL,
	    "Id"	    INTEGER NOT NULL,
	    "Type"	    INTEGER NOT NULL,
	    "Opt"	    INTEGER NOT NULL,
	    "TimeLimit"	INTEGER DEFAULT 0 NOT NULL,
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE,
        UNIQUE ("PlayerId", "Slot")
        );

        CREATE TABLE IF NOT EXISTS "Nanos" (
	    "PlayerId"	INTEGER NOT NULL,
	    "Id"	    INTEGER NOT NULL,
	    "Skill"	    INTEGER NOT NULL,
	    "Stamina"	INTEGER DEFAULT 150 NOT NULL,
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE,
        UNIQUE ("PlayerId", "Id")
        );

        CREATE TABLE IF NOT EXISTS "RunningQuests" (
	    "PlayerId"	            INTEGER NOT NULL,
	    "TaskId"	            INTEGER NOT NULL,
	    "RemainingNPCCount1"	INTEGER NOT NULL,
	    "RemainingNPCCount2"	INTEGER NOT NULL,
	    "RemainingNPCCount3"	INTEGER NOT NULL,
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS "Buddyships" (
	    "PlayerAId"	INTEGER NOT NULL,
	    "PlayerBId"	INTEGER NOT NULL,
	    "Status"	INTEGER NOT NULL,
        FOREIGN KEY("PlayerAId") REFERENCES "Players"("PlayerID") ON DELETE CASCADE,
        FOREIGN KEY("PlayerBId") REFERENCES "Players"("PlayerID") ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS "EmailData" (
	    "PlayerId"	INTEGER NOT NULL,
	    "MsgIndex"	INTEGER NOT NULL,
	    "ReadFlag"	INTEGER NOT NULL,
	    "ItemFlag"	INTEGER NOT NULL,
	    "SenderId"	INTEGER NOT NULL,
	    "SenderFirstName"	TEXT NOT NULL COLLATE NOCASE,
	    "SenderLastName"	TEXT NOT NULL COLLATE NOCASE,
	    "SubjectLine"	    TEXT NOT NULL,
	    "MsgBody"	        TEXT NOT NULL,
	    "Taros"	        INTEGER NOT NULL,
	    "SendTime"	    INTEGER NOT NULL,
	    "DeleteTime"	INTEGER NOT NULL,
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS "EmailItems" (
	    "PlayerId"	INTEGER NOT NULL,
	    "MsgIndex"	INTEGER NOT NULL,
	    "Slot"	    INTEGER NOT NULL,
	    "Id"	    INTEGER NOT NULL,
	    "Type"	    INTEGER NOT NULL,
	    "Opt"	    INTEGER NOT NULL,
	    "TimeLimit"	INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS "RaceResults"(
        "EPID"      INTEGER NOT NULL,
        "PlayerID"  INTEGER NOT NULL,
        "Score"     INTEGER NOT NULL,
        "Timestamp" INTEGER NOT NULL,    
        FOREIGN KEY("PlayerID") REFERENCES "Players"("PlayerID") ON DELETE CASCADE
        )
        )";

    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        std::cout << "[FATAL] Database failed to create tables: " << errMsg << std::endl;
        terminate(0);
    }
}

int Database::getTableSize(std::string tableName) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = "SELECT COUNT(*) FROM ?;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, 0);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

int Database::addAccount(std::string login, std::string password) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO "Accounts"
        ("Login", "Password")
        VALUES (?, ?);
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 2, BCrypt::generateHash(password).c_str(), -1, 0);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc==SQLITE_DONE ? sqlite3_last_insert_rowid(db) : 0;
}

void Database::updateSelected(int accountId, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);

    if (slot < 1 || slot > 4) {
        std::cout << "[WARN] Invalid slot number passed to updateSelected()! " << std::endl;
        return;
    }

    const char* sql = R"(
        UPDATE "Accounts"
        SET "Selected" = ?, "LastLogin" = (strftime('%s', 'now'))
        WHERE "AccountID" = ?;
        )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, slot);
    sqlite3_bind_int(stmt, 2, accountId);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
        std::cout << "[WARN] Database fail on updateSelected(). Error Code " << rc << std::endl;
    sqlite3_finalize(stmt);
}

void Database::findAccount(Account* account, std::string login) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT "AccountID", "Password", "Selected"
        FROM "Accounts"
        WHERE "Login" = ?
        LIMIT 1;        
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, login.c_str(), -1, 0);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        account->AccountID = sqlite3_column_int(stmt, 0);
        account->Password = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        account->Selected = sqlite3_column_int(stmt, 2);
    }
    sqlite3_finalize(stmt);
}

bool Database::validateCharacter(int characterID, int userID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    // query whatever
    const char* sql = R"(
        SELECT "PlayerID"
        FROM "Players"
        WHERE "PlayerID" = ? AND "AccountID" = ?
        LIMIT 1;        
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
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
        SELECT "PlayerID"
        FROM "Players"
        WHERE "Firstname" = ? AND "LastName" = ?
        LIMIT 1;        
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, firstName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 2, lastName.c_str(),  -1, 0);
    int rc = sqlite3_step(stmt);

    bool result = (rc != SQLITE_ROW);
    sqlite3_finalize(stmt);
    return result;
}

bool Database::isSlotFree(int accountId, int slotNum) {
    std::lock_guard<std::mutex> lock(dbCrit);

    if (slotNum < 1 || slotNum > 4) {
        std::cout << "[WARN] Invalid slot number passed to isSlotFree()! " << std::endl;
        return false;
    }

    const char* sql = R"(
        SELECT "PlayerID"
        FROM "Players"
        WHERE "AccountID" = ? AND "Slot" = ?
        LIMIT 1;        
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, accountId);
    sqlite3_bind_int(stmt, 2, slotNum);
    int rc = sqlite3_step(stmt);

    bool result = (rc != SQLITE_ROW);
    sqlite3_finalize(stmt);
    return result;
}

int Database::createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID) {
    std::lock_guard<std::mutex> lock(dbCrit);
    
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const char* sql = R"(
        INSERT INTO "Players"
        ("AccountID", "Slot", "Firstname", "LastName", "XCoordinates" , "YCoordinates", "ZCoordinates", "Angle",
         "HP", "NameCheck", "AccountLevel", "Quests")
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;
    std::string firstName = U16toU8(save->szFirstName);
    std::string lastName =  U16toU8(save->szLastName);
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, AccountID);
    sqlite3_bind_int(stmt, 2, save->iSlotNum);
    sqlite3_bind_text(stmt, 3, firstName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 4, lastName.c_str(), -1, 0);
    sqlite3_bind_int(stmt, 5, settings::SPAWN_X);
    sqlite3_bind_int(stmt, 6, settings::SPAWN_Y);
    sqlite3_bind_int(stmt, 7, settings::SPAWN_Z);
    sqlite3_bind_int(stmt, 8, settings::SPAWN_ANGLE);
    sqlite3_bind_int(stmt, 9, PC_MAXHEALTH(1));

    // if FNCode isn't 0, it's a wheel name
    int nameCheck = (settings::APPROVEALLNAMES || save->iFNCode) ? 1 : 0;
    sqlite3_bind_int(stmt, 10, nameCheck);
    sqlite3_bind_int(stmt, 11, settings::ACCLEVEL);

    // 128 byte blob for completed quests
    unsigned char blobBuffer[128] = { 0 };
    sqlite3_bind_blob(stmt, 12, blobBuffer, sizeof(blobBuffer), 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return 0;
    }

    int playerId = sqlite3_last_insert_rowid(db);
    
    sql = R"(
        INSERT INTO "Appearances"
        ("PlayerID")
        VALUES (?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return 0;
    }

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    return playerId;
}

bool Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character) {
    std::lock_guard<std::mutex> lock(dbCrit);
    
    const char* sql = R"(
        SELECT "PlayerID"
        FROM "Players"
        WHERE "PlayerID" = ? AND "AppearanceFlag" = 0
        LIMIT 1;        
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, character->PCStyle.iPC_UID);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if ( rc != SQLITE_ROW)
    {
        std::cout << "[WARN] Player tried Character Creation on already existing character?!" << std::endl;    
        return false;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    sql = R"(
        UPDATE "Players"
        SET "AppearanceFlag" = 1
        WHERE "PlayerID" = ?;
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, character->PCStyle.iPC_UID);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return false;
    }

    sql = R"(
        UPDATE "Appearances"
        SET
        "Body" = ? ,
        "EyeColor" = ? ,
        "FaceStyle" = ? ,
        "Gender" = ? ,
        "HairColor" = ? ,
        "HairStyle" = ? ,
        "Height" = ? ,
        "SkinColor" = ?
        WHERE "PlayerID" = ? ;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_int(stmt, 1, character->PCStyle.iBody);
    sqlite3_bind_int(stmt, 2, character->PCStyle.iEyeColor);
    sqlite3_bind_int(stmt, 3, character->PCStyle.iFaceStyle);
    sqlite3_bind_int(stmt, 4, character->PCStyle.iGender);
    sqlite3_bind_int(stmt, 5, character->PCStyle.iHairColor);
    sqlite3_bind_int(stmt, 6, character->PCStyle.iHairStyle);
    sqlite3_bind_int(stmt, 7, character->PCStyle.iHeight);
    sqlite3_bind_int(stmt, 8, character->PCStyle.iSkinColor);
    sqlite3_bind_int(stmt, 9, character->PCStyle.iPC_UID);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return false;
    }

    sql = R"(
        INSERT INTO "Inventory"
        ("PlayerID", "Slot", "Id", "Type", "Opt")
        VALUES (?, ?, ?, ?, 1);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    int items[3] = { character->sOn_Item.iEquipUBID, character->sOn_Item.iEquipLBID, character->sOn_Item.iEquipFootID };
    for (int i = 0; i < 3; i++) {
        sqlite3_bind_int(stmt, 1, character->PCStyle.iPC_UID);
        sqlite3_bind_int(stmt, 2, i+1);
        sqlite3_bind_int(stmt, 3, items[i]);
        sqlite3_bind_int(stmt, 4, i+1);

        rc = sqlite3_step(stmt);       

        if (rc != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            return false;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    return true;
}

bool Database::finishTutorial(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
    SELECT "PlayerID"
    FROM "Players"
    WHERE "PlayerID" = ? AND "TutorialFlag" = 0
    LIMIT 1;        
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerID);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        std::cout << "[WARN] Player tried to finish tutorial on a character that already did?!" << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    // Lightning Gun
    sql = R"(
    INSERT INTO "Inventory"
    ("PlayerID", "Slot", "Id", "Type", "Opt")
    VALUES (?, ?, ?, ?, 1);
    )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_bind_int(stmt, 2, 0);
    sqlite3_bind_int(stmt, 3, 328);
    sqlite3_bind_int(stmt, 4, 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return false;
    }

    // Nano Buttercup

    sql = R"(
    INSERT INTO "Nanos"
    ("PlayerID", "Id", "Skill")
    VALUES (?, ?, ?);
    )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_bind_int(stmt, 2, 1);
    sqlite3_bind_int(stmt, 3, 1);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return false;
    }

    sql = R"(
    UPDATE "Players"
    SET "TutorialFlag" = 1, 
    "Nano1" = 1,
    "Quests" = ?
    WHERE "PlayerID" = ?;
    )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    // save missions nr 1 & 2
    unsigned char questBuffer[128] = { 0 };
    questBuffer[0] = 3;
    sqlite3_bind_blob(stmt, 1, questBuffer, sizeof(questBuffer), 0);
    sqlite3_bind_int(stmt, 2, playerID);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        return false;
    }

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    return true;
}

int Database::deleteCharacter(int characterID, int userID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT "Slot"
        FROM "Players"
        WHERE "AccountID" = ? AND "PlayerID" = ?
        LIMIT 1;
        )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userID);
    sqlite3_bind_int(stmt, 2, characterID);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return 0;
    }
    int slot = sqlite3_column_int(stmt, 0);

    sql = R"(
        DELETE FROM "Players"
        WHERE "AccountID" = ? AND "PlayerID" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, userID);
    sqlite3_bind_int(stmt, 2, characterID);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return 0;

    return slot;
}

std::vector <sP_LS2CL_REP_CHAR_INFO> Database::getCharInfo(int userID) {
    std::vector<sP_LS2CL_REP_CHAR_INFO> result = std::vector<sP_LS2CL_REP_CHAR_INFO>();
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT p.PlayerID, p.Slot, p.Firstname, p.LastName, p.Level, p.AppearanceFlag, p.TutorialFlag, p.PayZoneFlag,
        p.XCoordinates, p.YCoordinates, p.ZCoordinates, p.NameCheck,
        a.Body, a.EyeColor, a.FaceStyle, a.Gender, a.HairColor, a.HairStyle, a.Height, a.SkinColor  
        FROM "Players" as p 
        INNER JOIN "Appearances" as a ON p.PlayerID = a.PlayerID
        WHERE p.AccountID = ?
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
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
            SELECT "Slot", "Type", "Id", "Opt", "TimeLimit" from Inventory
            WHERE "PlayerID" = ? AND "Slot"<9
            )";
        sqlite3_stmt* stmt2;

        sqlite3_prepare_v2(db, sql2, -1, &stmt2, 0);
        sqlite3_bind_int(stmt2, 1, toAdd.sPC_Style.iPC_UID);

        while (sqlite3_step(stmt2) == SQLITE_ROW) {
            sItemBase* item = &toAdd.aEquip[sqlite3_column_int(stmt2, 0)];
            item->iType = sqlite3_column_int(stmt2, 1);
            item->iID = sqlite3_column_int(stmt2, 2);
            item->iOpt = sqlite3_column_int(stmt2, 3);
            item->iTimeLimit = sqlite3_column_int(stmt2, 4);
        }
        sqlite3_finalize(stmt2);

        result.push_back(toAdd);
    }
    sqlite3_finalize(stmt);
    return result;

}

// XXX: This is never called?
void Database::evaluateCustomName(int characterID, CustomName decision) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
    UPDATE "Players"
    SET "NameCheck" = ?
    WHERE "PlayerID" = ?;
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, int(decision));
    sqlite3_bind_int(stmt, 2, characterID);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: Failed to update nameCheck" << std::endl;
    sqlite3_finalize(stmt);
}

bool Database::changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save, int accountId) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
    UPDATE "Players"
    SET "Firstname" = ?,
    "LastName" = ?
    WHERE "PlayerID" = ? AND "AccountID" = ?;
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, U16toU8(save->szFirstName).c_str(), -1, 0);
    sqlite3_bind_text(stmt, 2, U16toU8(save->szLastName).c_str(), -1, 0);
    sqlite3_bind_int(stmt, 3, save->iPCUID);
    sqlite3_bind_int(stmt, 4, accountId);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

Database::DbPlayer Database::playerToDb(Player *player) {
    // TODO: move stuff that is never updated to separate table so it doesn't try to update it every time
    DbPlayer result = {};

    result.PlayerID = player->iID;
    result.AccountID = player->accountId;
    result.AppearanceFlag = player->PCStyle2.iAppearanceFlag;
    result.Body = player->PCStyle.iBody;
    result.Class = player->PCStyle.iClass;
    result.EyeColor = player->PCStyle.iEyeColor;
    result.FaceStyle = player->PCStyle.iFaceStyle;
    result.FirstName = U16toU8( player->PCStyle.szFirstName);
    result.FusionMatter = player->fusionmatter;
    result.Gender = player->PCStyle.iGender;
    result.HairColor = player->PCStyle.iHairColor;
    result.HairStyle = player->PCStyle.iHairStyle;
    result.Height = player->PCStyle.iHeight;
    result.HP = player->HP;
    result.AccountLevel = player->accountLevel;
    result.LastName = U16toU8(player->PCStyle.szLastName);
    result.Level = player->level;
    result.NameCheck = player->PCStyle.iNameCheck;
    result.PayZoneFlag = player->PCStyle2.iPayzoneFlag;
    result.PlayerID = player->PCStyle.iPC_UID;
    result.SkinColor = player->PCStyle.iSkinColor;
    result.slot = player->slot;
    result.Taros = player->money;
    result.TutorialFlag = player->PCStyle2.iTutorialFlag;
    if (player->instanceID == 0 && !player->onMonkey) { // only save coords if player isn't instanced
        result.x_coordinates = player->x;
        result.y_coordinates = player->y;
        result.z_coordinates = player->z;
        result.angle = player->angle;
    } else {
        result.x_coordinates = player->lastX;
        result.y_coordinates = player->lastY;
        result.z_coordinates = player->lastZ;
        result.angle = player->lastAngle;
    }
    result.Nano1 = player->equippedNanos[0];
    result.Nano2 = player->equippedNanos[1];
    result.Nano3 = player->equippedNanos[2];
    result.BatteryN = player->batteryN;
    result.BatteryW = player->batteryW;
    result.Mentor = player->mentor;
    result.WarpLocationFlag = player->iWarpLocationFlag;
    result.SkywayLocationFlag1 = player->aSkywayLocationFlag[0];
    result.SkywayLocationFlag2 = player->aSkywayLocationFlag[1];
    result.CurrentMissionID = player->CurrentMissionID;

    // timestamp
    result.LastLogin = getTimestamp();
    result.Created = player->creationTime;

    // save completed quests
    result.QuestFlag = std::vector<char>((char*)player->aQuestFlag, (char*)player->aQuestFlag + 128);

    return result;
}

Player Database::DbToPlayer(DbPlayer player) {
    Player result = {}; // fixes some weird memory errors, this zeros out the members (not the padding inbetween though)

    result.iID = player.PlayerID;
    result.accountId = player.AccountID;
    result.creationTime = player.Created;
    result.PCStyle2.iAppearanceFlag = player.AppearanceFlag;
    result.PCStyle.iBody = player.Body;
    result.PCStyle.iClass = player.Class;
    result.PCStyle.iEyeColor = player.EyeColor;
    result.PCStyle.iFaceStyle = player.FaceStyle;
    U8toU16(player.FirstName, result.PCStyle.szFirstName, sizeof(result.PCStyle.szFirstName));
    result.PCStyle.iGender = player.Gender;
    result.PCStyle.iHairColor = player.HairColor;
    result.PCStyle.iHairStyle = player.HairStyle;
    result.PCStyle.iHeight = player.Height;
    result.HP = player.HP;
    result.accountLevel = player.AccountLevel;
    U8toU16(player.LastName, result.PCStyle.szLastName, sizeof(result.PCStyle.szLastName));
    result.level = player.Level;
    result.PCStyle.iNameCheck = player.NameCheck;
    result.PCStyle2.iPayzoneFlag = player.PayZoneFlag;
    result.iID = player.PlayerID;
    result.PCStyle.iPC_UID = player.PlayerID;
    result.PCStyle.iSkinColor = player.SkinColor;
    result.slot = player.slot;
    result.PCStyle2.iTutorialFlag = player.TutorialFlag;
    result.x = player.x_coordinates;
    result.y = player.y_coordinates;
    result.z = player.z_coordinates;
    result.angle = player.angle;
    result.money = player.Taros;
    result.fusionmatter = player.FusionMatter;
    result.batteryN = player.BatteryN;
    result.batteryW = player.BatteryW;
    result.mentor = player.Mentor;
    result.CurrentMissionID = player.CurrentMissionID;

    result.equippedNanos[0] = player.Nano1;
    result.equippedNanos[1] = player.Nano2;
    result.equippedNanos[2] = player.Nano3;

    result.inCombat = false;

    result.iWarpLocationFlag = player.WarpLocationFlag;
    result.aSkywayLocationFlag[0] = player.SkywayLocationFlag1;
    result.aSkywayLocationFlag[1] = player.SkywayLocationFlag2;

    Database::getInventory(&result);
    Database::removeExpiredVehicles(&result);
    Database::getNanos(&result);
    Database::getQuests(&result);
    Database::getBuddies(&result);

    // load completed quests
    memcpy(&result.aQuestFlag, player.QuestFlag.data(), std::min(sizeof(result.aQuestFlag), player.QuestFlag.size()));

    return result;
}

Database::DbPlayer Database::getDbPlayerById(int id) {
    auto player = db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == id));
    if (player.size() < 1) {
        // garbage collection
        db.remove_all<Inventory>(where(c(&Inventory::playerId) == id));
        db.remove_all<Nano>(where(c(&Nano::playerId) == id));
        db.remove_all<DbQuest>(where(c(&DbQuest::PlayerId) == id));
        db.remove_all<Buddyship>(where(c(&Buddyship::PlayerAId) == id || c(&Buddyship::PlayerBId) == id));
        db.remove_all<EmailData>(where(c(&EmailData::PlayerId) == id));
        db.remove_all<EmailItem>(where(c(&EmailItem::PlayerId) == id));
        return DbPlayer{ -1 };
    }
    return player.front();
}

Player Database::getPlayer(int id) {
    return DbToPlayer(
        getDbPlayerById(id)
    );
}

#pragma endregion LoginServer

#pragma region ShardServer

void Database::updatePlayer(Player *player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    DbPlayer toUpdate = playerToDb(player);
    db.update(toUpdate);
    updateInventory(player);
    updateNanos(player);
    updateQuests(player);
    //updateBuddies(player); we add/remove buddies explicitly now
}

void Database::updateInventory(Player *player){
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<Inventory>(
        where(c(&Inventory::playerId) == player->iID)
        );
    // insert equip
    for (int i = 0; i < AEQUIP_COUNT; i++) {
        if (player->Equip[i].iID != 0) {
            sItemBase* next = &player->Equip[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    // insert inventory
    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iID != 0) {
            sItemBase* next = &player->Inven[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    // insert bank
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iID != 0) {
            sItemBase* next = &player->Bank[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT + AINVEN_COUNT;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    // insert quest items
    for (int i = 0; i < AQINVEN_COUNT; i++) {
        if (player->QInven[i].iID != 0) {
            sItemBase* next = &player->QInven[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    db.commit();
}

void Database::updateNanos(Player *player) {
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<Nano>(
        where(c(&Nano::playerId) == player->iID)
        );
    // insert
    for (int i=1; i < SIZEOF_NANO_BANK_SLOT; i++) {
        if ((player->Nanos[i]).iID == 0)
            continue;
        Nano toAdd = {};
        sNano* next = &player->Nanos[i];
        toAdd.playerId = player->iID;
        toAdd.iID = next->iID;
        toAdd.iSkillID = next->iSkillID;
        toAdd.iStamina = next->iStamina;
        db.insert(toAdd);
    }
    db.commit();
}

void Database::updateQuests(Player* player) {
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<DbQuest>(
        where(c(&DbQuest::PlayerId) == player->iID)
        );
    // insert
    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++) {
        if (player->tasks[i] == 0)
            continue;
        DbQuest toAdd = {};
        toAdd.PlayerId = player->iID;
        toAdd.TaskId = player->tasks[i];
        toAdd.RemainingNPCCount1 = player->RemainingNPCCount[i][0];
        toAdd.RemainingNPCCount2 = player->RemainingNPCCount[i][1];
        toAdd.RemainingNPCCount3 = player->RemainingNPCCount[i][2];
        db.insert(toAdd);
    }
    db.commit();
}

// note: do not use. explicitly add/remove records instead.
void Database::updateBuddies(Player* player) {
    db.begin_transaction();

    db.remove_all<Buddyship>( // remove all buddyships with this player involved
        where(c(&Buddyship::PlayerAId) == player->iID || c(&Buddyship::PlayerBId) == player->iID)
        );

    // iterate through player's buddies and add records for each non-zero entry
    for (int i = 0; i < 50; i++) {
        if (player->buddyIDs[i] != 0) {
            Buddyship record;
            record.PlayerAId = player->iID;
            record.PlayerBId = player->buddyIDs[i];
            record.Status = 0; // still not sure how we'll handle blocking
            db.insert(record);
        }
    }

    db.commit();
}

void Database::getInventory(Player* player) {
    // get items from DB
    auto items = db.get_all<Inventory>(
        where(c(&Inventory::playerId) == player->iID)
        );
    // set items
    for (const Inventory &current : items) {
        sItemBase toSet = {};
        toSet.iID = current.id;
        toSet.iType = current.Type;
        toSet.iOpt = current.Opt;
        toSet.iTimeLimit = current.TimeLimit;
        // assign to proper arrays
        if (current.slot < AEQUIP_COUNT)
            player->Equip[current.slot] = toSet;
        else if (current.slot < (AEQUIP_COUNT + AINVEN_COUNT))
            player->Inven[current.slot - AEQUIP_COUNT] = toSet;
        else if (current.slot < (AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT))
            player->Bank[current.slot - AEQUIP_COUNT - AINVEN_COUNT] = toSet;
        else
            player->QInven[current.slot - AEQUIP_COUNT - AINVEN_COUNT - ABANK_COUNT] = toSet;
    }


}

void Database::removeExpiredVehicles(Player* player) {
    int32_t currentTime = getTimestamp();
    // remove from bank immediately
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iType == 10 && player->Bank[i].iTimeLimit < currentTime)
            player->Bank[i] = {};
    }
    // for the rest, we want to leave only 1 expired vehicle on player to delete it with the client packet
    std::vector<sItemBase*> toRemove;

    // equiped vehicle
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

void Database::getNanos(Player* player) {
    // get from DB
    auto nanos = db.get_all<Nano>(
        where(c(&Nano::playerId) == player->iID)
        );
    // set
    for (const Nano& current : nanos) {
        sNano *toSet = &player->Nanos[current.iID];
        toSet->iID = current.iID;
        toSet->iSkillID = current.iSkillID;
        toSet->iStamina = current.iStamina;
    }
}

void Database::getQuests(Player* player) {
    // get from DB
    auto quests = db.get_all<DbQuest>(
        where(c(&DbQuest::PlayerId) == player->iID)
        );
    // set
    int i = 0;
    for (const DbQuest& current : quests) {
        player->tasks[i] = current.TaskId;
        player->RemainingNPCCount[i][0] = current.RemainingNPCCount1;
        player->RemainingNPCCount[i][1] = current.RemainingNPCCount2;
        player->RemainingNPCCount[i][2] = current.RemainingNPCCount3;
        i++;
    }
}

void Database::getBuddies(Player* player) {
    auto buddies = db.get_all<Buddyship>( // player can be on either side
        where(c(&Buddyship::PlayerAId) == player->iID || c(&Buddyship::PlayerBId) == player->iID)
        );

    // there should never be more than 50 buddyships per player, but just in case
    for (int i = 0; i < 50 && i < buddies.size(); i++) {
        // if the player is player A, then the buddy is player B, and vice versa
        player->buddyIDs[i] = player->iID == buddies.at(i).PlayerAId
            ? buddies.at(i).PlayerBId : buddies.at(i).PlayerAId;
    }
}

int Database::getNumBuddies(Player* player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    auto buddies = db.get_all<Buddyship>( // player can be on either side
        where(c(&Buddyship::PlayerAId) == player->iID || c(&Buddyship::PlayerBId) == player->iID)
        );

    // again, for peace of mind
    return buddies.size() > 50 ? 50 : buddies.size();
}

// buddies
void Database::addBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    Buddyship record;
    record.PlayerAId = playerA;
    record.PlayerBId = playerB;
    record.Status = 0; // blocking ???
    db.insert(record);

    db.commit();
}

void Database::removeBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    db.remove_all<Buddyship>(
        where(c(&Buddyship::PlayerAId) == playerA && c(&Buddyship::PlayerBId) == playerB)
        );

    db.remove_all<Buddyship>( // the pair could be in either position
        where(c(&Buddyship::PlayerAId) == playerB && c(&Buddyship::PlayerBId) == playerA)
        );

    db.commit();
}

// email
int Database::getUnreadEmailCount(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    auto emailData = db.get_all<Database::EmailData>(
        where(c(&Database::EmailData::PlayerId) == playerID && c(&Database::EmailData::ReadFlag) == 0)
        );

    return emailData.size();
}

std::vector<Database::EmailData> Database::getEmails(int playerID, int page) {
    std::lock_guard<std::mutex> lock(dbCrit);
    
    std::vector<Database::EmailData> emails;

    auto emailData = db.get_all<Database::EmailData>(
        where(c(&Database::EmailData::PlayerId) == playerID),
        order_by(&Database::EmailData::MsgIndex).desc(),
        limit(5 * (page - 1), 5)
        );

    int i = 0;
    for (Database::EmailData email : emailData) {
        emails.push_back(email);
        i++;
    }

    return emails;
}

Database::EmailData Database::getEmail(int playerID, int index) {
    std::lock_guard<std::mutex> lock(dbCrit);

    auto emailData = db.get_all<Database::EmailData>(
        where(c(&Database::EmailData::PlayerId) == playerID && c(&Database::EmailData::MsgIndex) == index)
        );

    if (emailData.size() > 1)
        std::cout << "[WARN] Duplicate emails detected (player " << playerID << ", index " << index << ")" << std::endl;

    return emailData.at(0);
}

sItemBase* Database::getEmailAttachments(int playerID, int index) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sItemBase* items = new sItemBase[4];
    for (int i = 0; i < 4; i++)
        items[i] = { 0, 0, 0, 0 }; // zero out items

    auto attachments = db.get_all<Database::EmailItem>(
        where(c(&Database::EmailItem::PlayerId) == playerID && c(&Database::EmailItem::MsgIndex) == index)
        );

    if (attachments.size() > 4)
        std::cout << "[WARN] Email has too many attachments (player " << playerID << ", index " << index << ")" << std::endl;

    for (Database::EmailItem& item : attachments) {
        items[item.Slot - 1] = { item.Type, item.Id, item.Opt, item.TimeLimit };
    }

    return items;
}

void Database::updateEmailContent(EmailData* data) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    auto attachments = db.get_all<Database::EmailItem>(
        where(c(&Database::EmailItem::PlayerId) == data->PlayerId && c(&Database::EmailItem::MsgIndex) == data->MsgIndex)
        );
    data->ItemFlag = (data->Taros > 0 || attachments.size() > 0) ? 1 : 0; // set attachment flag dynamically

    db.remove_all<Database::EmailData>(
        where(c(&Database::EmailData::PlayerId) == data->PlayerId && c(&Database::EmailData::MsgIndex) == data->MsgIndex)
        );
    db.insert(*data);

    db.commit();
}

void Database::deleteEmailAttachments(int playerID, int index, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    if (slot == -1) { // delete all
        db.remove_all<Database::EmailItem>(
            where(c(&Database::EmailItem::PlayerId) == playerID && c(&Database::EmailItem::MsgIndex) == index)
            );
    } else { // delete single by comparing slot num
        db.remove_all<Database::EmailItem>(
            where(c(&Database::EmailItem::PlayerId) == playerID && c(&Database::EmailItem::MsgIndex) == index
                && c(&Database::EmailItem::Slot) == slot)
            );
    }

    db.commit();
}

void Database::deleteEmails(int playerID, int64_t* indices) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    for (int i = 0; i < 5; i++) {
        db.remove_all<Database::EmailData>(
            where(c(&Database::EmailData::PlayerId) == playerID && c(&Database::EmailData::MsgIndex) == indices[i])
            ); // no need to check if the index is 0, since an email will never have index < 1
    }

    db.commit();
}

int Database::getNextEmailIndex(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    auto emailData = db.get_all<Database::EmailData>(
        where(c(&Database::EmailData::PlayerId) == playerID),
        order_by(&Database::EmailData::MsgIndex).desc(),
        limit(1)
        );

    return (emailData.size() > 0 ? emailData.at(0).MsgIndex + 1 : 1);
}

void Database::sendEmail(EmailData* data, std::vector<sItemBase> attachments) {
    std::lock_guard<std::mutex> lock(dbCrit);

    db.begin_transaction();

    db.insert(*data); // add email data to db
    // add email attachments to db email inventory
    int slot = 1;
    for (sItemBase item : attachments) {
        EmailItem dbItem = {
            data->PlayerId,
            data->MsgIndex,
            slot++,
            item.iType,
            item.iID,
            item.iOpt,
            item.iTimeLimit
        };
        db.insert(dbItem);
    }

    db.commit();
}

#pragma endregion ShardServer
