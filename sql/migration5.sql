BEGIN TRANSACTION;
-- New Columns
ALTER TABLE Accounts ADD Email TEXT DEFAULT '' NOT NULL;
ALTER TABLE Accounts ADD LastPasswordReset INTEGER DEFAULT 0 NOT NULL;
-- Update DB Version
UPDATE Meta SET Value = 6 WHERE Key = 'DatabaseVersion';
UPDATE Meta SET Value = strftime('%s', 'now') WHERE Key = 'LastMigration';
COMMIT;
