#pragma once
#include "CNStructs.hpp"
#include "Player.hpp"
#include <string>
#include <vector>

namespace Database {
#pragma region DatabaseStructs

    struct Account
    {
        int AccountID;
        std::string Login;
        std::string Password;
        int Selected;
    };
    struct Inventory
    {
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
    struct DbPlayer
    {
        int PlayerID;
        int AccountID;
        short int slot;
        std::string FirstName;
        std::string LastName;
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
        bool isGM;
        int FusionMatter;
        int Taros;
        int x_coordinates;
        int y_coordinates;
        int z_coordinates;
        int angle;
        short int PCState;
        std::vector<char> QuestFlag;
    };



#pragma endregion DatabaseStructs

    // handles migrations
    void open();
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

    void getInventory(Player* player);
    void getNanos(Player* player);
}
