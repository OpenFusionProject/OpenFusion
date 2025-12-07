#include "db/internal.hpp"

// Email-related DB interactions

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

std::vector<EmailData> Database::getEmails(int playerID, int page) {
    std::lock_guard<std::mutex> lock(dbCrit);

    std::vector<EmailData> emails;

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
        EmailData toAdd;
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

EmailData Database::getEmail(int playerID, int index) {
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

    EmailData result;
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

    // set attachment flag dynamically
    data->ItemFlag = (data->Taros > 0 || attachmentsCount > 0) ? 1 : 0;

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

static void _deleteEmailAttachments(int playerID, int index, int slot) {
    sqlite3_stmt* stmt;

    std::string sql(R"(
        DELETE FROM EmailItems
        WHERE PlayerID = ? AND MsgIndex = ?
        )");

    if (slot != -1)
        sql += " AND \"Slot\" = ?";
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

void Database::deleteEmailAttachments(int playerID, int index, int slot) {
    std::lock_guard<std::mutex> lock(dbCrit);
    _deleteEmailAttachments(playerID, index, slot);
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
        int64_t msgIndex = indices[i];
        sqlite3_bind_int(stmt, 1, playerID);
        sqlite3_bind_int64(stmt, 2, msgIndex);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cout << "[WARN] Database: Failed to delete an email: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmt);
        // delete all attachments
        _deleteEmailAttachments(playerID, msgIndex, -1);
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

bool Database::sendEmail(EmailData* data, std::vector<sItemBase> attachments, Player *sender) {
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
            // very likely the UNIQUE constraint failing due to
            // orphaned attachments from an old email.
            // try deleting them first
            _deleteEmailAttachments(data->PlayerId, data->MsgIndex, -1);

            // try again
            sqlite3_reset(stmt);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                // different error, give up
                std::cout << "[WARN] Database: Failed to send email: " << sqlite3_errmsg(db) << std::endl;
                sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
                sqlite3_finalize(stmt);
                return false;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);

    if (!_updatePlayer(sender)) {
        std::cout << "[WARN] Database: Failed to save player to database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_exec(db, "ROLLBACK TRANSACTION;", NULL, NULL, NULL);
        return false;
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return true;
}
