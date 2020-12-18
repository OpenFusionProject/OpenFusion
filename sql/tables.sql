CREATE TABLE IF NOT EXISTS Accounts (
            AccountID    INTEGER NOT NULL,
            Login        TEXT    NOT NULL UNIQUE,
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

        CREATE TABLE IF NOT EXISTS Players (
            PlayerID           INTEGER NOT NULL,
            AccountID          INTEGER NOT NULL,
            FirstName          TEXT NOT NULL COLLATE NOCASE,
            LastName           TEXT NOT NULL COLLATE NOCASE,
            NameCheck          INTEGER NOT NULL,
            Slot               INTEGER NOT NULL,
            Created            INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
            LastLogin          INTEGER DEFAULT (strftime('%s', 'now')) NOT NULL,
            Level              INTEGER DEFAULT 1 NOT NULL,
            Nano1              INTEGER DEFAULT 0 NOT NULL,
            Nano2              INTEGER DEFAULT 0 NOT NULL,
            Nano3              INTEGER DEFAULT 0 NOT NULL,
            AppearanceFlag     INTEGER DEFAULT 0 NOT NULL,
            TutorialFlag       INTEGER DEFAULT 0 NOT NULL,
            PayZoneFlag        INTEGER DEFAULT 0 NOT NULL,
            XCoordinates       INTEGER NOT NULL,
            YCoordinates       INTEGER NOT NULL,
            ZCoordinates       INTEGER NOT NULL,
            Angle              INTEGER NOT NULL,
            HP                 INTEGER NOT NULL,
            FusionMatter       INTEGER DEFAULT 0 NOT NULL,
            Taros              INTEGER DEFAULT 0 NOT NULL,
            BatteryW           INTEGER DEFAULT 0 NOT NULL,
            BatteryN           INTEGER DEFAULT 0 NOT NULL,
            Mentor             INTEGER DEFAULT 5 NOT NULL,
            CurrentMissionID   INTEGER DEFAULT 0 NOT NULL,
            WarpLocationFlag   INTEGER DEFAULT 0 NOT NULL,
            SkywayLocationFlag BLOB NOT NULL,
            FirstUseFlag       BLOB NOT NULL,
            Quests             BLOB NOT NULL,
            PRIMARY KEY(PlayerID AUTOINCREMENT),
            FOREIGN KEY(AccountID) REFERENCES Accounts(AccountID) ON DELETE CASCADE,
            UNIQUE (AccountID, Slot),
            UNIQUE (FirstName, LastName)
        );

        CREATE TABLE IF NOT EXISTS Appearances (
            PlayerID    INTEGER UNIQUE NOT NULL,
            Body        INTEGER DEFAULT 0 NOT NULL,
            EyeColor    INTEGER DEFAULT 1 NOT NULL,
            FaceStyle   INTEGER DEFAULT 1 NOT NULL,
            Gender      INTEGER DEFAULT 1 NOT NULL,
            HairColor   INTEGER DEFAULT 1 NOT NULL,
            HairStyle   INTEGER DEFAULT 1 NOT NULL,
            Height      INTEGER DEFAULT 0 NOT NULL,
            SkinColor   INTEGER DEFAULT 1 NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS Inventory (
            PlayerID    INTEGER NOT NULL,
            Slot        INTEGER NOT NULL,
            ID          INTEGER NOT NULL,
            Type        INTEGER NOT NULL,
            Opt         INTEGER NOT NULL,
            TimeLimit   INTEGER DEFAULT 0 NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            UNIQUE (PlayerID, Slot)
        );

        CREATE TABLE IF NOT EXISTS QuestItems (
            PlayerID    INTEGER NOT NULL,
            Slot        INTEGER NOT NULL,
            ID          INTEGER NOT NULL,
            Opt         INTEGER NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            UNIQUE (PlayerID, Slot)
        );

        CREATE TABLE IF NOT EXISTS Nanos (
            PlayerID    INTEGER NOT NULL,
            ID          INTEGER NOT NULL,
            Skill       INTEGER NOT NULL,
            Stamina     INTEGER DEFAULT 150 NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            UNIQUE (PlayerID, ID)
        );

        CREATE TABLE IF NOT EXISTS RunningQuests (
            PlayerID              INTEGER NOT NULL,
            TaskID                INTEGER NOT NULL,
            RemainingNPCCount1    INTEGER NOT NULL,
            RemainingNPCCount2    INTEGER NOT NULL,
            RemainingNPCCount3    INTEGER NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS Buddyships (
            PlayerAID    INTEGER NOT NULL,
            PlayerBID    INTEGER NOT NULL,
            FOREIGN KEY(PlayerAID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            FOREIGN KEY(PlayerBID) REFERENCES Players(PlayerID) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS Blocks (
            PlayerID           INTEGER NOT NULL,
            BlockedPlayerID    INTEGER NOT NULL,
            FOREIGN KEY(PlayerID)        REFERENCES Players(PlayerID) ON DELETE CASCADE,
            FOREIGN KEY(BlockedPlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE
        );

        CREATE TABLE IF NOT EXISTS EmailData (
            PlayerID        INTEGER NOT NULL,
            MsgIndex        INTEGER NOT NULL,
            ReadFlag        INTEGER NOT NULL,
            ItemFlag        INTEGER NOT NULL,
            SenderID        INTEGER NOT NULL,
            SenderFirstName TEXT NOT NULL COLLATE NOCASE,
            SenderLastName  TEXT NOT NULL COLLATE NOCASE,
            SubjectLine     TEXT NOT NULL,
            MsgBody         TEXT NOT NULL,
            Taros           INTEGER NOT NULL,
            SendTime        INTEGER NOT NULL,
            DeleteTime      INTEGER NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            UNIQUE(PlayerID, MsgIndex)
        );

        CREATE TABLE IF NOT EXISTS EmailItems (
            PlayerID    INTEGER NOT NULL,
            MsgIndex    INTEGER NOT NULL,
            Slot        INTEGER NOT NULL,
            ID          INTEGER NOT NULL,
            Type        INTEGER NOT NULL,
            Opt         INTEGER NOT NULL,
            TimeLimit   INTEGER NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE,
            UNIQUE (MsgIndex, Slot)
        );

        CREATE TABLE IF NOT EXISTS RaceResults(
            EPID      INTEGER NOT NULL,
            PlayerID  INTEGER NOT NULL,
            Score     INTEGER NOT NULL,
            RingCount INTEGER NOT NULL,
            Time      INTEGER NOT NULL,
            Timestamp INTEGER NOT NULL,
            FOREIGN KEY(PlayerID) REFERENCES Players(PlayerID) ON DELETE CASCADE
        );
