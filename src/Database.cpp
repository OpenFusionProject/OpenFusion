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

        CREATE TABLE IF NOT EXISTS "QuestItems" (
	    "PlayerId"	INTEGER NOT NULL,
	    "Slot"	    INTEGER NOT NULL,
	    "Id"	    INTEGER NOT NULL,
	    "Opt"	    INTEGER NOT NULL,
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
	    "TimeLimit"	INTEGER NOT NULL,
        UNIQUE ("MsgIndex", "Slot")
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
            WHERE "PlayerID" = ? AND "Slot"< ?
            )";
        sqlite3_stmt* stmt2;

        sqlite3_prepare_v2(db, sql2, -1, &stmt2, 0);
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

void Database::getPlayer(Player* plr, int id) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT p.AccountID, p.Slot, p.Firstname, p.LastName,
        p.Level, p.Nano1, p.Nano2, p.Nano3,
        p.AppearanceFlag, p.TutorialFlag, p.PayZoneFlag,
        p.XCoordinates, p.YCoordinates, p.ZCoordinates, p.NameCheck,
        p.Angle, p.HP, p.AccountLevel, p.FusionMatter, p.Taros, p.Quests,
        p.BatteryW, p.BatteryN, p.Mentor, p.WarpLocationFlag,
        p.SkywayLocationFlag1, p.SkywayLocationFlag2, p.CurrentMissionID
        a.Body, a.EyeColor, a.FaceStyle, a.Gender, a.HairColor, a.HairStyle, a.Height, a.SkinColor  
        FROM "Players" as p 
        INNER JOIN "Appearances" as a ON p.PlayerID = a.PlayerID
        WHERE p.PlayerID = ?
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        std::cout << "[WARN] Database: Failed to load character [" << id << "]" << std::endl;
        return;
    }

    plr->iID = id;
    plr->PCStyle.iPC_UID = id;

    plr->accountId = sqlite3_column_int(stmt, 0);
    plr->slot = sqlite3_column_int(stmt, 1);
    
    // parsing const unsigned char* to char16_t 
    std::string placeHolder = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
    U8toU16(placeHolder, plr->PCStyle.szFirstName , sizeof(plr->PCStyle.szFirstName));
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

    const void* questBuffer = plr->aQuestFlag;
    questBuffer = sqlite3_column_blob(stmt, 20);

    plr->batteryW = sqlite3_column_int(stmt, 21);
    plr->batteryN = sqlite3_column_int(stmt, 22);
    plr->mentor = sqlite3_column_int(stmt, 23);
    plr->iWarpLocationFlag = sqlite3_column_int(stmt, 24);
    
    plr->aSkywayLocationFlag[0] = sqlite3_column_int(stmt, 25);
    plr->aSkywayLocationFlag[1] = sqlite3_column_int(stmt, 26);
    plr->CurrentMissionID = sqlite3_column_int(stmt, 27);

    plr->PCStyle.iBody = sqlite3_column_int(stmt, 28);
    plr->PCStyle.iEyeColor = sqlite3_column_int(stmt, 29);
    plr->PCStyle.iFaceStyle = sqlite3_column_int(stmt, 30);
    plr->PCStyle.iGender = sqlite3_column_int(stmt, 31);
    plr->PCStyle.iHairColor = sqlite3_column_int(stmt, 32);
    plr->PCStyle.iHairStyle = sqlite3_column_int(stmt, 33);
    plr->PCStyle.iHeight = sqlite3_column_int(stmt, 34);
    plr->PCStyle.iSkinColor = sqlite3_column_int(stmt, 35);

    // get inventory

    sql = R"(
        SELECT "Slot", "Type", "Id", "Opt", "TimeLimit" from Inventory
        WHERE "PlayerID" = ? AND "Slot" < ?
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_int(stmt, 1, id);
    // we don't want bank items here
    sqlite3_bind_int(stmt, 2, AEQUIP_COUNT + AINVEN_COUNT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt, 0);

        sItemBase* item = slot < AEQUIP_COUNT ? &plr->Equip[slot] : &plr->Inven[slot = AEQUIP_COUNT];
        item->iType = sqlite3_column_int(stmt, 1);
        item->iID = sqlite3_column_int(stmt, 2);
        item->iOpt = sqlite3_column_int(stmt, 3);
        item->iTimeLimit = sqlite3_column_int(stmt, 4);
    }

    Database::removeExpiredVehicles(plr);
    
    // get quest inventory

    sql = R"(
        SELECT "Slot", "Id", "Opt" from QuestItems
        WHERE "PlayerID" = ?
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    sqlite3_bind_int(stmt, 1, id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int slot = sqlite3_column_int(stmt, 0);

        sItemBase* item = &plr->QInven[slot];
        item->iType = 8;
        item->iID = sqlite3_column_int(stmt, 1);
        item->iOpt = sqlite3_column_int(stmt, 2);
    }


    // get nanos
    sql = R"(
        SELECT "Id", "Skill", "Stamina" from "Nanos"
        WHERE "PlayerID" = ?
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        sNano* nano = &plr->Nanos[id];
        nano->iID = id;
        nano->iSkillID = sqlite3_column_int(stmt, 1);
        nano->iStamina = sqlite3_column_int(stmt, 2);
    }

    // get active quests
    sql = R"(
        SELECT "TaskId", "RemainingNPCCount1", "RemainingNPCCount2", "RemainingNPCCount3" from "RunningQuests"
        WHERE "PlayerID" = ?
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < ACTIVE_MISSION_COUNT) {
        plr->tasks[i] = sqlite3_column_int(stmt, 0);
        plr->RemainingNPCCount[i][0] = sqlite3_column_int(stmt, 1);
        plr->RemainingNPCCount[i][1] = sqlite3_column_int(stmt, 2);
        plr->RemainingNPCCount[i][2] = sqlite3_column_int(stmt, 3);
    }

    // get buddies
    sql = R"(
        SELECT "PlayerAId", "PlayerBId" from "Buddyships"
        WHERE "PlayerAId" = ? OR "PlayerBId" = ?
        )";

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_int(stmt, 2, id);

    i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < 50) {
        int PlayerAId = sqlite3_column_int(stmt, 0);
        int PlayerBId = sqlite3_column_int(stmt, 1);

        plr->buddyIDs[i] = id == PlayerAId ? PlayerBId : PlayerAId;
    }

    sqlite3_finalize(stmt);
}

void Database::updatePlayer(Player *player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const char* sql = R"(
        UPDATE "Players"
        SET
        "Level" = ? , "Nano1" = ?, "Nano2" = ?, "Nano3" = ?,
        "XCoordinates" = ?, "YCoordinates" = ?, "ZCoordinates" = ?,
        "Angle" = ?, "HP" = ?, "FusionMatter" = ?, "Taros" = ?, "Quests" = ?,
        "BatteryW" = ?, "BatteryN" = ?, "WarplocationFlag" = ?,
        "SkywayLocationFlag1" = ?, "SkywayLocationFlag2" = ?, "CurrentMissionID" = ?
        WHERE "PlayerID" = ?
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->level);
    sqlite3_bind_int(stmt, 2, player->equippedNanos[0]);
    sqlite3_bind_int(stmt, 3, player->equippedNanos[1]);
    sqlite3_bind_int(stmt, 4, player->equippedNanos[2]);
    sqlite3_bind_int(stmt, 5, player->x);
    sqlite3_bind_int(stmt, 6, player->y);
    sqlite3_bind_int(stmt, 7, player->z);
    sqlite3_bind_int(stmt, 8, player->angle);
    sqlite3_bind_int(stmt, 9, player->HP);
    sqlite3_bind_int(stmt, 10, player->fusionmatter);
    sqlite3_bind_int(stmt, 11, player->money);
    sqlite3_bind_blob(stmt, 12, player->aQuestFlag, sizeof(player->aQuestFlag), 0);
    sqlite3_bind_int(stmt, 13, player->batteryW);
    sqlite3_bind_int(stmt, 14, player->batteryN);
    sqlite3_bind_int(stmt, 15, player->iWarpLocationFlag);
    sqlite3_bind_int(stmt, 16, player->aSkywayLocationFlag[0]);
    sqlite3_bind_int(stmt, 17, player->aSkywayLocationFlag[1]);
    sqlite3_bind_int(stmt, 18, player->CurrentMissionID);
    sqlite3_bind_int(stmt, 19, player->iID);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
        return;
    }

    // update inventory
    sql = R"(
        DELETE FROM "Inventory" WHERE "PlayerID" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sql = R"(
        INSERT INTO "Inventory"
        ("PlayerID", "Slot", "Type", "Opt" "Id", "Timelimit")
        VALUES (?, ?, ?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    int i = -1;
    while (++i < AEQUIP_COUNT && player->Equip[i].iID != 0) {
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_int(stmt, 3, player->Equip[i].iType);
        sqlite3_bind_int(stmt, 4, player->Equip[i].iOpt);
        sqlite3_bind_int(stmt, 5, player->Equip[i].iID);
        sqlite3_bind_int(stmt, 6, player->Equip[i].iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
            return;
        }
        sqlite3_reset(stmt);
    }

    i = -1;
    while (++i < AINVEN_COUNT && player->Inven[i].iID != 0) {
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i + AEQUIP_COUNT);
        sqlite3_bind_int(stmt, 3, player->Inven[i].iType);
        sqlite3_bind_int(stmt, 4, player->Inven[i].iOpt);
        sqlite3_bind_int(stmt, 5, player->Inven[i].iID);
        sqlite3_bind_int(stmt, 6, player->Inven[i].iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
            return;
        }
        sqlite3_reset(stmt);
    }

    // Update Quest Inventory
    sql = R"(
        DELETE FROM "QuestItems" WHERE "PlayerID" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sql = R"(
        INSERT INTO "QuestItems"
        ("PlayerID", "Slot", "Opt" "Id")
        VALUES (?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    i = -1;
    while (++i < AQINVEN_COUNT && player->QInven[i].iID != 0) {
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_int(stmt, 3, player->QInven[i].iOpt);
        sqlite3_bind_int(stmt, 4, player->QInven[i].iID);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
            return;
        }
        sqlite3_reset(stmt);
    }

    // Update Nanos
    sql = R"(
        DELETE FROM "Nanos" WHERE "PlayerID" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sql = R"(
        INSERT INTO "Nanos"
        ("PlayerID", "Id", "SKill" "Stamina")
        VALUES (?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    i = -1;
    while (++i < SIZEOF_NANO_BANK_SLOT && player->Nanos[i].iID != 0) {
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, player->Nanos[i].iID);
        sqlite3_bind_int(stmt, 3, player->Nanos[i].iSkillID);
        sqlite3_bind_int(stmt, 4, player->Nanos[i].iStamina);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
            return;
        }
        sqlite3_reset(stmt);
    }

    // Update Running Quests
    sql = R"(
        DELETE FROM "RunningQuests" WHERE "PlayerID" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_step(stmt);

    sql = R"(
        INSERT INTO "RunningQuests"
        ("PlayerID", "TaskId", "RemainingNPCCount1", "RemainingNPCCount2", "RemainingNPCCount3")
        VALUES (?, ?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    i = -1;
    while (++i < ACTIVE_MISSION_COUNT && player->tasks[i] != 0) {
        sqlite3_bind_int(stmt, 1, player->iID);
        sqlite3_bind_int(stmt, 2, player->tasks[i]);
        sqlite3_bind_int(stmt, 3, player->RemainingNPCCount[i][0]);
        sqlite3_bind_int(stmt, 4, player->RemainingNPCCount[i][1]);
        sqlite3_bind_int(stmt, 5, player->RemainingNPCCount[i][2]);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            std::cout << "[WARN] Database: Failed to save player to database" << std::endl;
            return;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
}


// note: do not use. explicitly add/remove records instead.
void Database::updateBuddies(Player* player) {
    //db.begin_transaction();

    //db.remove_all<Buddyship>( // remove all buddyships with this player involved
    //    where(c(&Buddyship::PlayerAId) == player->iID || c(&Buddyship::PlayerBId) == player->iID)
    //    );

    //// iterate through player's buddies and add records for each non-zero entry
    //for (int i = 0; i < 50; i++) {
    //    if (player->buddyIDs[i] != 0) {
    //        Buddyship record;
    //        record.PlayerAId = player->iID;
    //        record.PlayerBId = player->buddyIDs[i];
    //        record.Status = 0; // still not sure how we'll handle blocking
    //        db.insert(record);
    //    }
    //}

    //db.commit();
}

void Database::removeExpiredVehicles(Player* player) {
    int32_t currentTime = getTimestamp();

    // we want to leave only 1 expired vehicle on player to delete it with the client packet
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

int Database::getNumBuddies(Player* player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*) FROM "Buddyships"
        WHERE "PlayerAId" = ? OR "PlayerBId" = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, player->iID);
    sqlite3_bind_int(stmt, 2, player->iID);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    // again, for peace of mind
    return result > 50 ? 50 : result;
}

// buddies
void Database::addBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO "Buddyships" 
        ("PlayerAId", "PlayerBId", Status")
        VALUES (?, ?, ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerA);
    sqlite3_bind_int(stmt, 2, playerB);
    // blocking???
    sqlite3_bind_int(stmt, 3, 0);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to add buddies" << std::endl;
    sqlite3_finalize(stmt);
}

void Database::removeBuddyship(int playerA, int playerB) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        DELETE FROM "Buddyships" 
        WHERE ("PlayerAId" = ? AND "PlayerBId" = ?) OR ("PlayerAId" = ? AND "PlayerBId" = ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerA);
    sqlite3_bind_int(stmt, 2, playerB);
    sqlite3_bind_int(stmt, 3, playerB);
    sqlite3_bind_int(stmt, 4, playerA);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to remove buddies" << std::endl;
    sqlite3_finalize(stmt);
}

// email
int Database::getUnreadEmailCount(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT (*) FROM "EmailData"
        WHERE "PlayerId" = ? AND "ReadFlag" = 0;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_step(stmt);
    return sqlite3_column_int(stmt, 0);
}

std::vector<Database::EmailData> Database::getEmails(int playerID, int page) {
    std::lock_guard<std::mutex> lock(dbCrit);
    
    std::vector<Database::EmailData> emails;

    const char* sql = R"(
        SELECT "MsgIndex", "ItemFlag", "ReadFlag", "SenderId", "SenderFirstName", "SenderLastName", "SubjectLine", "MsgBody", 
        "Taros", "SendTime", "DeleteTime" 
        FROM "EmailData" 
        WHERE "PlayerId" = ?
        ORDER BY "MsgIndex" DESC
        LIMIT 5
        OFFSET ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
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
        SELECT "ItemFlag", "ReadFlag", "SenderId", "SenderFirstName", "SenderLastName", "SubjectLine", "MsgBody", 
        "Taros", "SendTime", "DeleteTime" 
        FROM "EmailData" 
        WHERE "PlayerId" = ? AND "MsgIndex" = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
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

    if (sqlite3_step(stmt) == SQLITE_ROW)
        std::cout << "[WARN] Database: Player has multiple emails with the same index ?!" << std::endl;

    sqlite3_finalize(stmt);
    return result;
}

sItemBase* Database::getEmailAttachments(int playerID, int index) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sItemBase* items = new sItemBase[4];
    for (int i = 0; i < 4; i++)
        items[i] = { 0, 0, 0, 0 };

    const char* sql = R"(
        SELECT "Slot", "Id", "Type", "Opt", "TimeLimit" 
        FROM "EmailItems"
        WHERE "PlayerId" = ? AND "MsgIndex" = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
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
    
    return items;
}

void Database::updateEmailContent(EmailData* data) {
    std::lock_guard<std::mutex> lock(dbCrit);
    
    const char* sql = R"(
        SELECT COUNT(*) FROM "EmailItems"
        WHERE "PlayerId" = ? AND "MsgIndex" = ?;
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_step(stmt);
    int attachmentsCount = sqlite3_column_int(stmt, 0);

    data->ItemFlag = (data->Taros > 0 || attachmentsCount > 0) ? 1 : 0; // set attachment flag dynamically
    // TODO: CHANGE THIS TO UPDATE
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const char* sql = R"(
        DELETE FROM "EmailData"
        WHERE "PlayerId" = ? AND "MsgIndex" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_step(stmt);

    const char* sql = R"(
        INSERT INTO "EmailData" 
        ("PlayerId", "MsgIndex", "ReadFlag", "ItemFlag", "SenderId", "SenderFirstName", "SenderLastName",
        "SubjectLine", "MsgBody", "Taros", "SendTime", "DeleteTime"
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_bind_int(stmt, 3, data->ReadFlag);
    sqlite3_bind_int(stmt, 4, data->ItemFlag);
    sqlite3_bind_int(stmt, 5, data->SenderId);
    sqlite3_bind_text(stmt, 6, data->SenderFirstName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 7, data->SenderLastName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 8, data->SubjectLine.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 9, data->MsgBody.c_str(), -1, 0);
    sqlite3_bind_int(stmt, 10, data->Taros);
    sqlite3_bind_int64(stmt, 11, data->SendTime);
    sqlite3_bind_int64(stmt, 12, data->DeleteTime);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: failed to update email" << std::endl;

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
}

void Database::deleteEmailAttachments(int playerID, int index, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_stmt* stmt;

    if (slot == -1) { // delete all
        const char* sql = R"(
            DELETE FROM "EmailItems"
            WHERE "PlayerId" = ? AND "MsgIndex" = ?;
            )";
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, playerID);
        sqlite3_bind_int(stmt, 2, index);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            std::cout << "[wARN] Database: Failed to delete email attachemtns" << std::endl;
    
    } else { // delete single by comparing slot num
        const char* sql = R"(
            DELETE FROM "EmailItems"
            WHERE "PlayerId" = ? AND "MsgIndex" = ? AND "Slot" = ?;
            )";
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, playerID);
        sqlite3_bind_int(stmt, 2, index);
        sqlite3_bind_int(stmt, 3, slot);
        if (sqlite3_step(stmt) != SQLITE_DONE)
            std::cout << "[wARN] Database: Failed to delete email attachemtns" << std::endl;
    }
    sqlite3_finalize(stmt);
}

void Database::deleteEmails(int playerID, int64_t* indices) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
    sqlite3_stmt* stmt;

    const char* sql = R"(
        DELETE FROM "EmailData"
        WHERE "PlayerId" = ? AND "MsgIndex" = ?;
        )";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    for (int i = 0; i < 5; i++) {
        sqlite3_bind_int(stmt, 1, playerID);
        sqlite3_bind_int64(stmt, 2, indices[i]);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
}

int Database::getNextEmailIndex(int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT "MsgIndex" FROM "EmailData"
        WHERE "PlayerId" = ?
        ORDER BY "MsgIndex" DESC
        LIMIT 1
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, playerID);
    sqlite3_step(stmt);
    int index = sqlite3_column_int(stmt, 0);

    return (index > 0 ? index + 1 : 1);
}

void Database::sendEmail(EmailData* data, std::vector<sItemBase> attachments) {
    std::lock_guard<std::mutex> lock(dbCrit);

    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const char* sql = R"(
        INSERT INTO "EmailData" 
        ("PlayerId", "MsgIndex", "ReadFlag", "ItemFlag", "SenderId", "SenderFirstName", "SenderLastName",
        "SubjectLine", "MsgBody", "Taros", "SendTime", "DeleteTime"
        VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, data->PlayerId);
    sqlite3_bind_int(stmt, 2, data->MsgIndex);
    sqlite3_bind_int(stmt, 3, data->ReadFlag);
    sqlite3_bind_int(stmt, 4, data->ItemFlag);
    sqlite3_bind_int(stmt, 5, data->SenderId);
    sqlite3_bind_text(stmt, 6, data->SenderFirstName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 7, data->SenderLastName.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 8, data->SubjectLine.c_str(), -1, 0);
    sqlite3_bind_text(stmt, 9, data->MsgBody.c_str(), -1, 0);
    sqlite3_bind_int(stmt, 10, data->Taros);
    sqlite3_bind_int64(stmt, 11, data->SendTime);
    sqlite3_bind_int64(stmt, 12, data->DeleteTime);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: Failed to send email" << std::endl;
        sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
        sqlite3_finalize(stmt);
        return;
    }

    int slot = 1;
    for (sItemBase item : attachments) {
        sql = R"(
            INSERT INTO EmailItems
            ("PlayerId", "MsgIndex", "Slot", "Id", "Type", "Opt", "TimeLimit")
            VALUES (?, ?, ?, ?, ?, ?, ?);
            )";
        sqlite3_stmt* stmt;

        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        sqlite3_bind_int(stmt, 1, data->PlayerId);
        sqlite3_bind_int(stmt, 2, data->MsgIndex);
        sqlite3_bind_int(stmt, 3, slot++);
        sqlite3_bind_int(stmt, 4, item.iID);
        sqlite3_bind_int(stmt, 5, item.iType);
        sqlite3_bind_int(stmt, 6, item.iOpt);
        sqlite3_bind_int(stmt, 7, item.iTimeLimit);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to send email" << std::endl;
            sqlite3_exec(db, "ROLLBACK TRANSACTION", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
}

