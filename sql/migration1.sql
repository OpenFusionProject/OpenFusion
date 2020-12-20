BEGIN TRANSACTION;
-- New Columns
ALTER TABLE Accounts ADD BanReason TEXT DEFAULT '' NOT NULL;
ALTER TABLE RaceResults ADD RingCount INTEGER DEFAULT 0 NOT NULL;
ALTER TABLE RaceResults ADD Time INTEGER DEFAULT 0 NOT NULL;
-- Fix timestamps in Meta
INSERT INTO Meta (Key, Value) VALUES ('Created', 0);
INSERT INTO Meta (Key, Value) VALUES ('LastMigration', strftime('%s', 'now'));
UPDATE Meta SET Value = (SELECT Created FROM Meta WHERE Key = 'ProtocolVersion') Where Key = 'Created';
-- Get rid of 'Created' Column
CREATE TABLE Temp(Key TEXT NOT NULL UNIQUE, Value INTEGER NOT NULL);
INSERT INTO Temp SELECT Key, Value FROM Meta;
DROP TABLE Meta;
ALTER TABLE Temp RENAME TO Meta;
-- Update DB Version
UPDATE Meta SET Value = 2 WHERE Key = 'DatabaseVersion';
UPDATE Meta SET Value = strftime('%s', 'now') WHERE Key = 'LastMigration';
COMMIT;
