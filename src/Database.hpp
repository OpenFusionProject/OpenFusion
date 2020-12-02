#pragma once
#include "CNStructs.hpp"
#include "Player.hpp"
#include <string>
#include <vector>

namespace Database {
#pragma region DatabaseStructs

    struct Account {
        int AccountID;
        std::string Password;
        int Selected;
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
    void createTables();

    int getTableSize(std::string tableName);
    // returns ID, 0 if something failed
    int addAccount(std::string login, std::string password);
    void updateSelected(int accountId, int playerId);
    void findAccount(Account* account, std::string login);
    bool validateCharacter(int characterID, int userID);
    bool isNameFree(std::string firstName, std::string lastName);
    bool isSlotFree(int accountId, int slotNum);
    // called after chosing name, returns ID
    int createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID);
    // called after finishing creation
    bool finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character);
    // called after tutorial
    bool finishTutorial(int playerID);
    // returns slot number
    int deleteCharacter(int characterID, int userID);
    std::vector <sP_LS2CL_REP_CHAR_INFO> getCharInfo(int userID);
    // accepting/declining custom name
    enum class CustomName {
        APPROVE = 1,
        DISAPPROVE = 2
    };
    void evaluateCustomName(int characterID, CustomName decision);
    bool changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save, int accountId);

    // getting players
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
