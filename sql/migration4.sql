/*
    It is recommended in the SQLite manual to turn off
    foreign keys when making schema changes that involve them
*/
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
-- New table to store auth cookies
CREATE TABLE Auth (
    AccountID   INTEGER NOT NULL,
    Cookie      TEXT NOT NULL,
    Valid       INTEGER NOT NULL,
    FOREIGN KEY(AccountID) REFERENCES Accounts(AccountID) ON DELETE CASCADE,
    UNIQUE (AccountID)
);
-- Update DB Version
UPDATE Meta SET Value = 5 WHERE Key = 'DatabaseVersion';
UPDATE Meta SET Value = strftime('%s', 'now') WHERE Key = 'LastMigration';
COMMIT;
PRAGMA foreign_keys=ON;