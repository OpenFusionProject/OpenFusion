#include "contrib/sqlite/sqlite3pp.h"
#include "contrib/bcrypt/BCrypt.hpp"
#include "CNProtocol.hpp"
#include "Database.hpp"
#include "CNStructs.hpp"
#include "settings.hpp"
#include "Player.hpp"
#include <regex>
#include <fstream>
#include "contrib/JSON.hpp"


//TODO: replace this sqlite wrapper with something better, clean up queries, get rid of json

sqlite3pp::database db;

void Database::open() {
    //checking if database file exists
    std::ifstream file;
    file.open("data.db");

    if (file) {
        file.close();
        // if exists, assign it		
        db = sqlite3pp::database("data.db");
        DEBUGLOG(std::cout << "[DB] Database in operation" << std::endl;)
    }
    else {
        // if doesn't, create all the tables
        DEBUGLOG(std::cout << "[DB] Creating new database" << std::endl;)
        db = sqlite3pp::database("data.db");

        // creates accounts
        db.execute("CREATE TABLE Accounts(AccountID INTEGER PRIMARY KEY AUTOINCREMENT, Login TEXT NOT NULL, Password TEXT NOT NULL);");
        // creates characters
        db.execute("CREATE TABLE Players(PlayerID INTEGER PRIMARY KEY AUTOINCREMENT, AccountID INTEGER NOT NULL, Slot INTEGER NOT NULL, FirstName TEXT NOT NULL, LastName TEXT NOT NULL, CharData TEXT NOT NULL);");

        DEBUGLOG(std::cout << "Done" << std::endl;)
    }
}

// verifies that the username & passwords are valid
bool Database::isLoginDataGood(std::string login, std::string password) {
    std::regex loginRegex("^([A-Za-z\\d_\\-]){5,20}$");
    std::regex passwordRegex("^([A-Za-z\\d_\\-@$!%*#?&,.+:;<=>]){2,20}$");
    return (std::regex_match(login, loginRegex) && std::regex_match(password, passwordRegex));
}

void Database::addAccount(std::string login, std::string password) {
    // generates prepared statment
    sqlite3pp::command cmd(db, "INSERT INTO Accounts (Login, Password) VALUES (:login, :password)");

    // generates a hashed password!
    password = BCrypt::generateHash(password);

    // binds args to the command
    cmd.bind(":login", login, sqlite3pp::nocopy);
    cmd.bind(":password", password, sqlite3pp::nocopy);
    cmd.execute();
}

bool Database::doesUserExist(std::string login) {
    // generates prepared statement
    sqlite3pp::query qry(db, "SELECT COUNT(AccountID) FROM Accounts WHERE Login = :login");
    // binds to the query
    qry.bind(":login", login, sqlite3pp::nocopy);

    // executes
    sqlite3pp::query::iterator i = qry.begin();

    // grabs the first result (the count)
    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    // if there is more than 0 results, the user exists :eyes:
    return (result > 0);
}

bool Database::isPasswordCorrect(std::string login, std::string password) {
    // generates prepared statement
    sqlite3pp::query qry(db, "SELECT Password FROM Accounts WHERE Login = :login");
    // binds username to the query
    qry.bind(":login", login, sqlite3pp::nocopy);

    // executes
    sqlite3pp::query::iterator i = qry.begin();

    // grabs the first result
    const char* actual;
    std::tie(actual) = (*i).get_columns<const char*>(0);

    // validate password hash with provded password
    return BCrypt::validatePassword(password, actual);
}

int Database::getUserID(std::string login) {
    // generates prep statement
    sqlite3pp::query qry(db, "SELECT AccountID FROM Accounts WHERE Login = :login");
    // binds the username to the login param
    qry.bind(":login", login, sqlite3pp::nocopy);

    // executes the query
    sqlite3pp::query::iterator i = qry.begin();

    // grabs the first result of the query
    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    // returns the result
    return result;
}

int Database::getUserSlotsNum(int AccountId) {
    // generates the prepared statement
    sqlite3pp::query qry(db, "SELECT COUNT(PlayerID) FROM Players WHERE AccountID = :ID");

    // binds the ID to the param
    qry.bind(":ID", AccountId);

    // executes
    sqlite3pp::query::iterator i = qry.begin();

    // grabs the first result
    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    // returns the result
    return result;
}

bool Database::isNameFree(std::string First, std::string Second) {
    // generates the prepared statement
    sqlite3pp::query qry(db, "SELECT COUNT(PlayerID) FROM Players WHERE FirstName = :First COLLATE nocase AND LastName = :Second COLLATE nocase");

    // binds the params
    qry.bind(":First", First, sqlite3pp::nocopy);
    qry.bind(":Second", Second, sqlite3pp::nocopy);

    // executes the query
    sqlite3pp::query::iterator i = qry.begin();

    // grabs the result
    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    // if no results return, the the username is unused
    return (result == 0);
}

void Database::createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID) {
    // generate the command
    sqlite3pp::command cmd(db, "INSERT INTO Players (AccountID, Slot, FirstName, LastName, CharData) VALUES (:AccountID, :Slot, :FirstName, :LastName, :CharData)");

    // generate character data
    std::string charData = CharacterToJson(save);
    std::string first = U16toU8(save->szFirstName);
    std::string last = U16toU8(save->szLastName);

    // bind to command
    cmd.bind(":AccountID", AccountID);
    cmd.bind(":Slot", save->iSlotNum);
    cmd.bind(":CharData", charData, sqlite3pp::nocopy);
    cmd.bind(":FirstName", first, sqlite3pp::nocopy);
    cmd.bind(":LastName", last, sqlite3pp::nocopy);

    // run
    cmd.execute();
}

void Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character) {
    sqlite3pp::command cmd(db, "UPDATE Players SET CharData = :data WHERE PlayerID = :id");

    // grab the data to add to the command
    int id = character->PCStyle.iPC_UID;
    std::string charData = CharacterToJson(character);

    // bind to the command & execute
    cmd.bind(":data", charData, sqlite3pp::nocopy);
    cmd.bind(":id", id);
    cmd.execute();
}

void Database::finishTutorial(int PlayerID) {
    sqlite3pp::query qry(db, "SELECT CharData FROM Players WHERE PlayerID = :ID");

    // bind to the query and execute
    qry.bind(":ID", PlayerID);
    sqlite3pp::query::iterator i = qry.begin();

    // grab the player json from the database
    std::string json;
    std::tie(json) = (*i).get_columns<std::string>(0);

    // parse the json into a Player 
    Player player = JsonToPlayer(json, PlayerID);

    // set the tutorial flag & equip lightning gun
    player.PCStyle2.iTutorialFlag = 1;
    player.Equip[0].iID = 328;
    json = PlayerToJson(player);

    // update the database
    sqlite3pp::command cmd(db, "UPDATE Players SET CharData = :data WHERE PlayerID = :id");
    cmd.bind(":data", json, sqlite3pp::nocopy);
    cmd.bind(":id", PlayerID);
    cmd.execute();
}

int Database::getCharacterID(int AccountID, int slot) {
    // make the query
    sqlite3pp::query qry(db, "SELECT PlayerID FROM Players WHERE AccountID = :ID AND Slot = :Slot");

    // bind the params & execute
    qry.bind(":ID", AccountID);
    qry.bind(":Slot", slot);
    sqlite3pp::query::iterator i = qry.begin();

    // grab the result
    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    return result;
}

int Database::deleteCharacter(int characterID, int accountID) {
    // checking if requested player exist and is bound to the account

    sqlite3pp::query qry(db, "SELECT COUNT(AccountID) FROM Players WHERE AccountID = :AccID AND PlayerID = :PID");

    qry.bind(":AccID", accountID);
    qry.bind(":PID", characterID);
    sqlite3pp::query::iterator i = qry.begin();

    int result;
    std::tie(result) = (*i).get_columns<int>(0);

    if (result > 0) {
        // get player character slot
        sqlite3pp::query qry(db, "SELECT Slot FROM Players WHERE PlayerID = :PID");

        // bind & execute
        qry.bind(":PID", characterID);
        sqlite3pp::query::iterator i = qry.begin();

        // grab the slot to return
        int slot;
        std::tie(slot) = (*i).get_columns<int>(0);

        // actually delete the record
        sqlite3pp::command cmd(db, "DELETE FROM Players WHERE PlayerID = :ID");
        cmd.bind(":ID", characterID);
        cmd.execute();

        // finally, return the grabbed slot
        return slot;
    }

    return -1;
}

// TODO: this should really be std::vector, but for the sake of compatibility with PRs, I'll change this after merging
std::list<Player> Database::getCharacters(int userID) {
    std::list<Player> result = std::list<Player>();

    sqlite3pp::query qry(db, "SELECT * FROM Players WHERE AccountID = :ID");

    qry.bind(":ID", userID);

    // for each character owned by the account, 
    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        // grab the data
        int ID, AccountID, slot;
        char const* charData, * first, * second;
        std::tie(ID, AccountID, slot, first, second, charData) =
            (*i).get_columns<int, int, int, char const*, char const*, char const*>(0, 1, 2, 3, 4, 5);

        // convert to player
        Player toadd = JsonToPlayer(charData, ID);
        toadd.slot = slot;

        // add it to the results
        result.push_back(toadd);
    }
    return result;
}

std::string Database::CharacterToJson(sP_CL2LS_REQ_SAVE_CHAR_NAME* save) {
    nlohmann::json json = {
        {"Level",36},
        //to check
        {"HP",1000},
        {"NameCheck", 1},
        {"FirstName",U16toU8(save->szFirstName)},
        {"LastName",U16toU8(save->szLastName)},
        {"Gender",rand() % 2 + 1 },
        {"FaceStyle",1},
        {"HairStyle",1},
        {"HairColor",(rand() % 19) + 1},
        {"SkinColor",(rand() % 13) + 1},
        {"EyeColor",(rand() % 6) + 1},
        {"Height",(rand() % 6)},
        {"Body",(rand() % 4)},
        {"Class",0},
        {"AppearanceFlag",0},
        {"PayzoneFlag",1},
        {"TutorialFlag",0},
        {"EquipUB", 217},
        {"EquipLB", 203},
        {"EquipFoot", 314},
        {"EquipWeapon", 0},
        {"x",settings::SPAWN_X},
        {"y",settings::SPAWN_Y},
        {"z",settings::SPAWN_Z},
        {"isGM",false},
    };
    return json.dump();
}

std::string Database::PlayerToJson(Player player) {
    nlohmann::json json = {
        {"Level",player.level},
        //to check
        {"HP",player.HP},
        {"NameCheck", 1},
        {"FirstName",U16toU8(player.PCStyle.szFirstName)},
        {"LastName",U16toU8(player.PCStyle.szLastName)},
        {"Gender",player.PCStyle.iGender},
        {"FaceStyle",player.PCStyle.iFaceStyle},
        {"HairStyle",player.PCStyle.iHairStyle},
        {"HairColor",player.PCStyle.iHairColor},
        {"SkinColor",player.PCStyle.iSkinColor},
        {"EyeColor",player.PCStyle.iEyeColor},
        {"Height",player.PCStyle.iHeight},
        {"Body",player.PCStyle.iBody},
        {"Class",player.PCStyle.iClass},
        {"AppearanceFlag",player.PCStyle2.iAppearanceFlag},
        {"PayzoneFlag",player.PCStyle2.iPayzoneFlag},
        {"TutorialFlag",player.PCStyle2.iTutorialFlag},
        {"EquipUB", player.Equip[1].iID},
        {"EquipLB", player.Equip[2].iID},
        {"EquipFoot", player.Equip[3].iID},
        {"EquipWeapon", player.Equip[0].iID},
        {"x",player.x},
        {"y",player.y},
        {"z",player.z},
        {"isGM",false},
    };
    return json.dump();
}

Player Database::JsonToPlayer(std::string input, int PC_UID) {
    std::string err;
    const auto json = nlohmann::json::parse(input);
    Player player;
    player.PCStyle.iPC_UID = (int64_t)PC_UID;
    player.level = std::stoi(json["Level"].dump());
    player.HP = std::stoi(json["HP"].dump());
    player.PCStyle.iNameCheck = std::stoi(json["NameCheck"].dump());
    U8toU16(json["FirstName"].get<std::string>(), player.PCStyle.szFirstName);
    U8toU16(json["LastName"].get<std::string>(), player.PCStyle.szLastName);
    player.PCStyle.iGender = std::stoi(json["Gender"].dump());
    player.PCStyle.iFaceStyle = std::stoi(json["FaceStyle"].dump());
    player.PCStyle.iHairStyle = std::stoi(json["HairStyle"].dump());
    player.PCStyle.iHairColor = std::stoi(json["HairColor"].dump());
    player.PCStyle.iSkinColor = std::stoi(json["SkinColor"].dump());
    player.PCStyle.iEyeColor = std::stoi(json["EyeColor"].dump());
    player.PCStyle.iHeight = std::stoi(json["Height"].dump());
    player.PCStyle.iBody = std::stoi(json["Body"].dump());
    player.PCStyle.iClass = std::stoi(json["Class"].dump());
    player.PCStyle2.iAppearanceFlag = std::stoi(json["AppearanceFlag"].dump());
    player.PCStyle2.iPayzoneFlag = std::stoi(json["PayzoneFlag"].dump());
    player.PCStyle2.iTutorialFlag = std::stoi(json["TutorialFlag"].dump());
    player.Equip[0].iID = std::stoi(json["EquipWeapon"].dump());
    if (player.Equip[0].iID != 0)
        player.Equip[0].iOpt = 1;
    else
        player.Equip[0].iOpt = 0;
    player.Equip[0].iType = 0;
    player.Equip[1].iID = std::stoi(json["EquipUB"].dump());
    player.Equip[1].iOpt = 1;
    player.Equip[1].iType = 1;
    player.Equip[2].iID = std::stoi(json["EquipLB"].dump());
    player.Equip[2].iOpt = 1;
    player.Equip[2].iType = 2;
    player.Equip[3].iID = std::stoi(json["EquipFoot"].dump());
    player.Equip[3].iOpt = 1;
    player.Equip[3].iType = 3;
    player.x = std::stoi(json["x"].dump());
    player.y = std::stoi(json["y"].dump());
    player.z = std::stoi(json["z"].dump());
    return player;
}

std::string Database::CharacterToJson(sP_CL2LS_REQ_CHAR_CREATE* character) {
    nlohmann::json json = {
        {"Level",36},
        //to check
        {"HP",1000},
        {"NameCheck", 1},
        {"FirstName",U16toU8(character->PCStyle.szFirstName)},
        {"LastName",U16toU8(character->PCStyle.szLastName)},
        {"Gender",character->PCStyle.iGender},
        {"FaceStyle",character->PCStyle.iFaceStyle},
        {"HairStyle",character->PCStyle.iHairStyle},
        {"HairColor",character->PCStyle.iHairColor},
        {"SkinColor",character->PCStyle.iSkinColor},
        {"EyeColor",character->PCStyle.iEyeColor},
        {"Height",character->PCStyle.iHeight},
        {"Body",character->PCStyle.iBody},
        {"Class",character->PCStyle.iClass},
        {"AppearanceFlag",1},
        {"PayzoneFlag",1},
        {"TutorialFlag",0},
        {"EquipUB", character->sOn_Item.iEquipUBID},
        {"EquipLB", character->sOn_Item.iEquipLBID},
        {"EquipFoot", character->sOn_Item.iEquipFootID},
        {"EquipWeapon", 0},
        {"x",settings::SPAWN_X},
        {"y",settings::SPAWN_Y},
        {"z",settings::SPAWN_Z},
        {"isGM",false},
    };
    return json.dump();
}
