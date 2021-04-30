/* 
    It is recommended in the SQLite manual to turn off
    foreign keys when making schema changes that involve them
*/
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
-- New table to store code items
CREATE TABLE RedeemedCodes(
    PlayerID    INTEGER NOT NULL,
    Code        TEXT NOT NULL,
    FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
    UNIQUE (PlayerID, Code)
);
-- Change Coordinates in Players table to non-plural form
ALTER TABLE Players RENAME COLUMN XCoordinates TO XCoordinate;
ALTER TABLE Players RENAME COLUMN YCoordinates TO YCoordinate;
ALTER TABLE Players RENAME COLUMN ZCoordinates TO ZCoordinate;
-- Fix email attachments not being unique enough
CREATE TABLE Temp (
    PlayerID    INTEGER NOT NULL,
    MsgIndex    INTEGER NOT NULL,
    Slot        INTEGER NOT NULL,
    ID          INTEGER NOT NULL,
    Type        INTEGER NOT NULL,
    Opt         INTEGER NOT NULL,
    TimeLimit   INTEGER NOT NULL,
    FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
    UNIQUE (PlayerID, MsgIndex, Slot)
);
INSERT INTO Temp SELECT * FROM EmailItems;
DROP TABLE EmailItems;
ALTER TABLE Temp RENAME TO EmailItems;
-- Update DB Version
UPDATE Meta SET Value = 3 WHERE Key = 'DatabaseVersion';
UPDATE Meta SET Value = strftime('%s', 'now') WHERE Key = 'LastMigration';
COMMIT;
PRAGMA foreign_keys=ON;
