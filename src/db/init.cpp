#include "db/internal.hpp"
#include "settings.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

std::mutex dbCrit;
sqlite3 *db;

static void createMetaTable() {
    std::lock_guard<std::mutex> lock(dbCrit); // XXX

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

static void checkMetaTable() {
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

static void createTables() {
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

static int getTableSize(std::string tableName) {
    std::lock_guard<std::mutex> lock(dbCrit); // XXX

    const char* sql = "SELECT COUNT(*) FROM ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, NULL);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

void Database::open() {
    // XXX: move locks here
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
