#include "db/internal.hpp"

#include <assert.h>

// Miscellanious in-game database interactions

static int getAccountIDFromPlayerID(int playerId, int *accountLevel=nullptr) {
    const char *sql = R"(
        SELECT Players.AccountID, AccountLevel
        FROM Players
        JOIN Accounts ON Players.AccountID = Accounts.AccountID
        WHERE PlayerID = ?;
        )";
    sqlite3_stmt *stmt;

    // get AccountID from PlayerID
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        std::cout << "[WARN] Database: failed to get AccountID from PlayerID: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return -1;
    }

    int accountId = sqlite3_column_int(stmt, 0);

    // optional secondary return value, for checking GM status
    if (accountLevel != nullptr)
        *accountLevel = sqlite3_column_int(stmt, 1);

    sqlite3_finalize(stmt);

    return accountId;
}

static bool banAccount(int accountId, int days, std::string& reason) {
    const char* sql = R"(
        UPDATE Accounts SET
            BannedSince = (strftime('%s', 'now')),
            BannedUntil = (strftime('%s', 'now')) + ?,
            BanReason = ?
        WHERE AccountID = ?;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, days * 86400); // convert days to seconds
    sqlite3_bind_text(stmt, 2, reason.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 3, accountId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: failed to ban account: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

static bool unbanAccount(int accountId) {
    const char* sql = R"(
        UPDATE Accounts SET
            BannedSince = 0,
            BannedUntil = 0,
            BanReason = ''
        WHERE AccountID = ?;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    sqlite3_bind_int(stmt, 1, accountId);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: failed to unban account: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// expressed in days
#define THIRTY_YEARS 10957

bool Database::banPlayer(int playerId, std::string& reason) {
    std::lock_guard<std::mutex> lock(dbCrit);

    int accountLevel;
    int accountId = getAccountIDFromPlayerID(playerId, &accountLevel);
    if (accountId < 0) {
        return false;
    }

    if (accountLevel <= 30) {
        std::cout << "[WARN] Cannot ban a GM." << std::endl;
        return false;
    }

    // do the ban
    if (!banAccount(accountId, THIRTY_YEARS, reason)) {
        return false;
    }

    return true;
}

bool Database::unbanPlayer(int playerId) {
    std::lock_guard<std::mutex> lock(dbCrit);

    int accountId = getAccountIDFromPlayerID(playerId);
    if (accountId < 0)
        return false;

    return unbanAccount(accountId);
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

RaceRanking Database::getTopRaceRanking(int epID, int playerID) {
    std::lock_guard<std::mutex> lock(dbCrit);
    std::string sql(R"(
        SELECT
            EPID, PlayerID, Score, RingCount, Time, Timestamp
        FROM RaceResults
        WHERE EPID = ?
        )");

    if (playerID > -1)
        sql += " AND PlayerID = ? ";

    sql += R"(
        ORDER BY Score DESC
        LIMIT 1;
        )";

    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, epID);
    if(playerID > -1)
        sqlite3_bind_int(stmt, 2, playerID);

    RaceRanking ranking = {};
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        // this race hasn't been run before, so return a blank ranking
        sqlite3_finalize(stmt);
        return ranking;
    }

    assert(epID == sqlite3_column_int(stmt, 0)); // EPIDs should always match

    ranking.EPID = epID;
    ranking.PlayerID = sqlite3_column_int(stmt, 1);
    ranking.Score = sqlite3_column_int(stmt, 2);
    ranking.RingCount = sqlite3_column_int(stmt, 3);
    ranking.Time = sqlite3_column_int64(stmt, 4);
    ranking.Timestamp = sqlite3_column_int64(stmt, 5);

    sqlite3_finalize(stmt);
    return ranking;
}

void Database::postRaceRanking(RaceRanking ranking) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO RaceResults
            (EPID, PlayerID, Score, RingCount, Time, Timestamp)
        VALUES(?, ?, ?, ?, ?, ?);
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, ranking.EPID);
    sqlite3_bind_int(stmt, 2, ranking.PlayerID);
    sqlite3_bind_int(stmt, 3, ranking.Score);
    sqlite3_bind_int(stmt, 4, ranking.RingCount);
    sqlite3_bind_int64(stmt, 5, ranking.Time);
    sqlite3_bind_int64(stmt, 6, ranking.Timestamp);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cout << "[WARN] Database: Failed to post race result" << std::endl;
    }

    sqlite3_finalize(stmt);
}

bool Database::isCodeRedeemed(int playerId, std::string code) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        SELECT COUNT(*)
        FROM RedeemedCodes
        WHERE PlayerID = ? AND Code = ?
        LIMIT 1;
        )";
    sqlite3_stmt* stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, NULL);
    sqlite3_step(stmt);
    int result = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return result;
}

void Database::recordCodeRedemption(int playerId, std::string code) {
    std::lock_guard<std::mutex> lock(dbCrit);

    const char* sql = R"(
        INSERT INTO RedeemedCodes (PlayerID, Code)
        VALUES (?, ?);
        )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, playerId);
    sqlite3_bind_text(stmt, 2, code.c_str(), -1, NULL);
    
    if (sqlite3_step(stmt) != SQLITE_DONE)
        std::cout << "[WARN] Database: recording of code redemption failed: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_finalize(stmt);
}
