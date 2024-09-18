#pragma once

#include "Player.hpp"

#include <string>
#include <vector>

#define DATABASE_VERSION 5

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

    struct RaceRanking {
        int EPID;
        int PlayerID;
        int Score;
        int RingCount;
        uint64_t Time;
        uint64_t Timestamp;
    };
    
    void init();
    void open();
    void close();

    void findAccount(Account* account, std::string login);

    // return ID, 0 if something failed
    int getAccountIdForPlayer(int playerId);
    int addAccount(std::string login, std::string password);

    void updateAccountLevel(int accountId, int accountLevel);

    // return true if cookie is valid for the account.
    // invalidates the stored cookie afterwards
    bool checkCookie(int accountId, const char *cookie);

    // interface for the /ban command
    bool banPlayer(int playerId, std::string& reason);
    bool unbanPlayer(int playerId);

    void updateSelected(int accountId, int slot);
    void updateSelectedByPlayerId(int accountId, int playerId);
    
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
    bool _updatePlayer(Player *player);
    void updatePlayer(Player *player);
    void commitTrade(Player *plr1, Player *plr2);
    
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
    bool sendEmail(EmailData* data, std::vector<sItemBase> attachments, Player *sender);

    // racing
    RaceRanking getTopRaceRanking(int epID, int playerID);
    void postRaceRanking(RaceRanking ranking);

    // code items
    bool isCodeRedeemed(int playerId, std::string code);
    void recordCodeRedemption(int playerId, std::string code);
}
