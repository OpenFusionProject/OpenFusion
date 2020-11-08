#pragma once
#include "CNStructs.hpp"
#include "Player.hpp"
#include <string>
#include <vector>

namespace Database {
#pragma region DatabaseStructs

    struct Account {
        int AccountID;
        std::string Login;
        std::string Password;
        int Selected;
        uint64_t Created;
        uint64_t LastLogin;
    };
    struct Inventory {
        int playerId;
        int slot;
        int16_t Type;
        int16_t id;
        int32_t Opt;
        int32_t TimeLimit;
    };
    struct Nano {
        int playerId;
        int16_t iID;
        int16_t iSkillID;
        int16_t iStamina;
    };
    struct DbPlayer {
        int PlayerID;
        int AccountID;
        short int slot;
        std::string FirstName;
        std::string LastName;
        uint64_t Created;
        uint64_t LastLogin;
        short int Level;
        int Nano1;
        int Nano2;
        int Nano3;
        short int AppearanceFlag;
        short int Body;
        short int Class;
        short int EyeColor;
        short int FaceStyle;
        short int Gender;
        int HP;
        short int HairColor;
        short int HairStyle;
        short int Height;
        short int NameCheck;
        short int PayZoneFlag;
        short int SkinColor;
        bool TutorialFlag;
        int AccountLevel;
        int FusionMatter;
        int Taros;
        int x_coordinates;
        int y_coordinates;
        int z_coordinates;
        int angle;
        short int PCState;
        int BatteryW;
        int BatteryN;
        int16_t Mentor;
        std::vector<char> QuestFlag;
        int32_t CurrentMissionID;
        int32_t WarpLocationFlag;
        int64_t SkywayLocationFlag1;
        int64_t SkywayLocationFlag2;
    };
    struct DbQuest {
        int PlayerId;
        int32_t TaskId;
        int RemainingNPCCount1;
        int RemainingNPCCount2;
        int RemainingNPCCount3;
    };
    struct Buddyship {
        int PlayerAId;
        int PlayerBId;
        int16_t Status;
    };
    struct EmailData {
        int PlayerId;
        int MsgIndex;
        int32_t ReadFlag;
        int32_t ItemFlag;
        int SenderId;
        std::string SenderFirstName;
        std::string SenderLastName;
        std::string SubjectLine;
        std::string MsgBody;
        int Taros;
        uint64_t SendTime;
        uint64_t DeleteTime;
    };
    struct EmailItem {
        int PlayerId;
        int MsgIndex;
        int Slot;
        int16_t Type;
        int16_t Id;
        int32_t Opt;
        int32_t TimeLimit;
    };


#pragma endregion DatabaseStructs

    // handles migrations
    void open();
    int getAccountsCount();
    int getPlayersCount();
    // returns ID
    int addAccount(std::string login, std::string password);
    void updateSelected(int accountId, int playerId);
    std::unique_ptr<Account> findAccount(std::string login);
    bool isNameFree(sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck);
    // called after chosing name, returns ID
    int createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID);
    // called after finishing creation
    void finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character);
    // called after tutorial
    void finishTutorial(int PlayerID);
    // returns slot number
    int deleteCharacter(int characterID, int userID);
    std::vector <Player> getCharacters(int userID);
    // accepting/declining custom name
    enum class CustomName {
        APPROVE = 1,
        DISAPPROVE = 2
    };
    void evaluateCustomName(int characterID, CustomName decision);
    void changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save);

    // parsing DbPlayer
    DbPlayer playerToDb(Player *player);
    Player DbToPlayer(DbPlayer player);

    // getting players
    DbPlayer getDbPlayerById(int id);
    Player getPlayer(int id);

    void updatePlayer(Player *player);
    void updateInventory(Player *player);
    void updateNanos(Player *player);
    void updateQuests(Player* player);
    void updateBuddies(Player* player);

    void getInventory(Player* player);
    void removeExpiredVehicles(Player* player);
    void getNanos(Player* player);
    void getQuests(Player* player);
    void getBuddies(Player* player);
    int getNumBuddies(Player* player);

    // parsing blobs
    void appendBlob(std::vector<char>*blob, int64_t input);
    int64_t blobToInt64(std::vector<char>::iterator it);

    // buddies
    void addBuddyship(int playerA, int playerB);
    void removeBuddyship(int playerA, int playerB);

    // email
    int getUnreadEmailCount(int playerID);
    std::vector<EmailData> getEmails(int playerID, int page);
    EmailData getEmail(int playerID, int index);
    sItemBase* getEmailAttachments(int playerID, int index);
    void updateEmailContent(EmailData* data);
    void deleteEmailAttachments(int playerID, int index, int slot);
    void deleteEmails(int playerID, int64_t* indices);
    int getNextEmailIndex(int playerID);
    void sendEmail(EmailData* data, std::vector<sItemBase> attachments);
}
