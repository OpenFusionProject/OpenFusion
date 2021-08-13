/*
    It is recommended in the SQLite manual to turn off
    foreign keys when making schema changes that involve them
*/
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
-- Change username column (Login) to be case-insensitive
CREATE TABLE Temp (
    AccountID    INTEGER NOT NULL,
    Login        TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    Password     TEXT    NOT NULL,
    Selected     INTEGER  DEFAULT 1 NOT NULL,
    AccountLevel INTEGER NOT NULL,
    Created      INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
    LastLogin    INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
    BannedUntil  INTEGER DEFAULT 0 NOT NULL,
    BannedSince  INTEGER DEFAULT 0 NOT NULL,
    BanReason    TEXT    DEFAULT '' NOT NULL,
    PRIMARY KEY(AccountID AUTOINCREMENT)
);
INSERT INTO Temp SELECT * FROM Accounts;
DROP TABLE Accounts;
ALTER TABLE Temp RENAME TO Accounts;
-- Update DB Version
UPDATE Meta SET Value = 4 WHERE Key = 'DatabaseVersion';
UPDATE Meta SET Value = strftime('%s', 'now') WHERE Key = 'LastMigration';
COMMIT;
PRAGMA foreign_keys=ON;
