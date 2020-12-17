#pragma once
#include "CNStructs.hpp"
#include "Player.hpp"
#include <string>
#include <vector>

#define DATABASE_VERSION 2

namespace Database {

    struct Account {
        int AccountID;
        std::string Password;
        int Selected;
        time_t BannedUntil;
        std::string BanReason;
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
    
    void open();
    void close();
    void checkMetaTable();
    void createMetaTable();
    void createTables();
    int getTableSize(std::string tableName);

    void findAccount(Account* account, std::string login);
    /// returns ID, 0 if something failed
    int addAccount(std::string login, std::string password);
    void banAccount(int accountId, int days);
    void updateSelected(int accountId, int playerId);
    
    bool validateCharacter(int characterID, int userID);
    bool isNameFree(std::string firstName, std::string lastName);
    bool isSlotFree(int accountId, int slotNum);
    /// returns ID, 0 if something failed
    int createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID);
    /// returns true if query succeeded
    bool finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character, int accountId);
    /// returns true if query succeeded
    bool finishTutorial(int playerID, int accountID);
    /// returns slot number if query succeeded
    int deleteCharacter(int characterID, int userID);
    void getCharInfo(std::vector <sP_LS2CL_REP_CHAR_INFO>* result, int userID);
    /// accepting/declining custom name
    enum class CustomName {
        APPROVE = 1,
        DISAPPROVE = 2
    };
    void evaluateCustomName(int characterID, CustomName decision);
    /// returns true if query succeeded
    bool changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save, int accountId);

    // getting players
    void getPlayer(Player* plr, int id);
    void updatePlayer(Player *player);
    void removeExpiredVehicles(Player* player);
    
    // buddies
    int getNumBuddies(Player* player);
    void addBuddyship(int playerA, int playerB);
    void removeBuddyship(int playerA, int playerB);
    
    // blocking
    void addBlock(int playerId, int blockedPlayerId);
    void removeBlock(int playerId, int blockedPlayerId);

    // email
    int getUnreadEmailCount(int playerID);
    std::vector<EmailData> getEmails(int playerID, int page);
    EmailData getEmail(int playerID, int index);
    sItemBase* getEmailAttachments(int playerID, int index);
    void updateEmailContent(EmailData* data);
    void deleteEmailAttachments(int playerID, int index, int slot);
    void deleteEmails(int playerID, int64_t* indices);
    int getNextEmailIndex(int playerID);
    bool sendEmail(EmailData* data, std::vector<sItemBase> attachments);
}
