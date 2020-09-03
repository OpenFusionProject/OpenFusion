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
        int AccountID;
    };
    struct DbPlayer
    {
        int PlayerID;
        int AccountID;
        short int slot;
        std::string FirstName;
        std::string LastName;
        short int AppearanceFlag;
        short int Body;
        short int Class;
        short int EquipFoot;
        short int EquipLB;
        short int EquipUB;
        short int EquipWeapon1;
        short int EquipWeapon2;
        short int EyeColor;
        short int FaceStyle;
        short int Gender;
        int HP;
        short int HairColor;
        short int HairStyle;
        short int Height;
        short int Level;
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
    };

    

#pragma endregion DatabaseStructs

    //handles migrations
    void open();
    //returns ID
    int addAccount(std::string login, std::string password);
    void updateSelected(int accountId, int playerId);
    std::unique_ptr<Account> findAccount(std::string login);
    bool isNameFree(sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck);
    //called after chosing name, returns ID
    int createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID);
    //called after finishing creation
    void finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character);
    //called after tutorial
    void finishTutorial(int PlayerID);
    //returns slot number
    int deleteCharacter(int characterID);
    std::vector <Player> getCharacters(int userID);
    //accepting/declining custom name
    enum class CUSTOMNAME {
        approve = 1,
        disapprove = 2
    };
    void evaluateCustomName(int characterID, CUSTOMNAME decision);
    void changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save);

    //parsing DbPlayer
    DbPlayer playerToDb(Player player);
    Player DbToPlayer(DbPlayer player);

    //getting players
    DbPlayer getDbPlayerById(int id);

}
