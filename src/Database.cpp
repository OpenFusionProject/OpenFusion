#include "Database.hpp"
#include "contrib/bcrypt/BCrypt.hpp">
#include "CNProtocol.hpp"
#include <string>
#include "contrib/JSON.hpp"
#include "CNStructs.hpp"
#include "settings.hpp"
#include "Player.hpp"
#include "CNStructs.hpp"
#include "contrib/sqlite/sqlite_orm.h"

using namespace sqlite_orm;

# pragma region DatabaseScheme
auto db = make_storage("database.db",
    make_table("Accounts",
        make_column("AccountID", &Database::Account::AccountID, autoincrement(), primary_key()),
        make_column("Login", &Database::Account::Login),
        make_column("Password", &Database::Account::Password),
        make_column("Selected", &Database::Account::Selected)
    ),
    make_table("Players",
        make_column("PlayerID", &Database::DbPlayer::PlayerID, autoincrement(), primary_key()),
        make_column("AccountID", &Database::DbPlayer::AccountID),
        make_column("Slot", &Database::DbPlayer::slot),
        make_column("Firstname", &Database::DbPlayer::FirstName),
        make_column("LastName", &Database::DbPlayer::LastName),
        make_column("Level", &Database::DbPlayer::Level),
        make_column("AppearanceFlag", &Database::DbPlayer::AppearanceFlag),
        make_column("TutorialFlag", &Database::DbPlayer::TutorialFlag),
        make_column("PayZoneFlag", &Database::DbPlayer::PayZoneFlag),
        make_column("XCoordinates", &Database::DbPlayer::x_coordinates),
        make_column("YCoordinates", &Database::DbPlayer::y_coordinates),
        make_column("ZCoordinates", &Database::DbPlayer::z_coordinates), 
        make_column("Angle", &Database::DbPlayer::angle),
        make_column("Body", &Database::DbPlayer::Body),
        make_column("Class", &Database::DbPlayer::Class),
        make_column("EquipFoot", &Database::DbPlayer::EquipFoot),
        make_column("EquipLB", &Database::DbPlayer::EquipLB),
        make_column("EquipUB", &Database::DbPlayer::EquipUB),
        make_column("EquipWeapon1", &Database::DbPlayer::EquipWeapon1),
        make_column("EyeColor", &Database::DbPlayer::EyeColor),
        make_column("FaceStyle", &Database::DbPlayer::FaceStyle),
        make_column("Gender", &Database::DbPlayer::Gender),
        make_column("HP", &Database::DbPlayer::HP),
        make_column("HairColor", &Database::DbPlayer::HairColor),
        make_column("HairStyle", &Database::DbPlayer::HairStyle),
        make_column("Height", &Database::DbPlayer::Height),        
        make_column("NameCheck", &Database::DbPlayer::NameCheck),        
        make_column("SkinColor", &Database::DbPlayer::SkinColor),        
        make_column("isGM", &Database::DbPlayer::isGM),
        make_column("FusionMatter", &Database::DbPlayer::FusionMatter),
        make_column("Taros", &Database::DbPlayer::Taros),
        make_column("PCState", &Database::DbPlayer::PCState)
    ),
    make_table("Inventory",
        make_column("AccountID", &Database::Inventory::AccountID, primary_key())
    )
);

# pragma endregion DatabaseScheme

#pragma region LoginServer

void Database::open() 
{
    //this parameter means it will try to preserve data during migration
    bool preserve = true;
    db.sync_schema(preserve);
    DEBUGLOG(
        std::cout << "[DB] Database in operation" << std::endl;
    )
}

int Database::addAccount(std::string login, std::string password) 
{
    password = BCrypt::generateHash(password);
    Account x;
    x.Login = login;
    x.Password = password;
    x.Selected = 1;
    return db.insert(x);
}

void Database::updateSelected(int accountId, int slot)
{
    Account acc = db.get<Account>(accountId);
    acc.Selected = slot;
    db.update(acc);
}

std::unique_ptr<Database::Account> Database::findAccount(std::string login) 
{
    //this is awful, I've tried everything to improve it
    auto find = db.get_all<Account>(
        where(c(&Account::Login) == login), limit(1));
    if (find.empty())
        return nullptr;
    return
        std::unique_ptr<Account>(new Account(find.front()));    
}

bool Database::isNameFree(sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck)
{
    //TODO: add colate nocase
    std::string First = U16toU8(nameCheck->szFirstName);
    std::string Last = U16toU8(nameCheck->szLastName);
    return
        (db.get_all<DbPlayer>
            (where((c(&DbPlayer::FirstName) == First)
                and (c(&DbPlayer::LastName) == Last)))
            .empty());                
}

int Database::createCharacter(sP_CL2LS_REQ_SAVE_CHAR_NAME* save, int AccountID) 
{
    DbPlayer create;
    //save packet data
    create.FirstName = U16toU8(save->szFirstName);
    create.LastName = U16toU8(save->szLastName);
    create.slot = save->iSlotNum;
    create.AccountID = AccountID;
    //set flags
    create.AppearanceFlag = 0;
    create.TutorialFlag = 0;
    create.PayZoneFlag = 0;
    //set namecheck based on setting
    if (settings::APPROVEALLNAMES || save->iFNCode)
        create.NameCheck = 1;
    else
        create.NameCheck = 0;
    //create default body character
    create.Body= 0;
    create.Class= 0;
    create.EquipFoot= 0;
    create.EquipLB= 0;
    create.EquipUB= 0;
    create.EquipWeapon1= 0;
    create.EquipWeapon2= 0;
    create.EyeColor= 1;
    create.FaceStyle= 1;
    create.Gender= 1;
    create.HP= 1000;
    create.HairColor= 1;
    create.HairStyle = 1;
    create.Height= 0;
    create.Level= 1;
    create.SkinColor= 1;
    create.isGM = false;
     //commented and disabled for now
    //if (U16toU8(save->szFirstName) == settings::GMPASS) {
    //    create.isGM = true;
    //}
    
    create.FusionMatter= 0;
    create.Taros= 0;
    create.PCState = 0;
    create.x_coordinates = settings::SPAWN_X;
    create.y_coordinates= settings::SPAWN_Y;
    create.z_coordinates= settings::SPAWN_Z;
    create.angle = 0;
    return db.insert(create);
}

void Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character) 
{
    DbPlayer finish = getDbPlayerById(character->PCStyle.iPC_UID);
    finish.AppearanceFlag = 1;
    finish.Body = character->PCStyle.iBody;
    finish.Class = character->PCStyle.iClass;
    finish.EquipFoot = character->sOn_Item.iEquipFootID;
    finish.EquipLB = character->sOn_Item.iEquipLBID;
    finish.EquipUB = character->sOn_Item.iEquipUBID;
    finish.EyeColor = character->PCStyle.iEyeColor;
    finish.FaceStyle = character->PCStyle.iFaceStyle;
    finish.Gender = character->PCStyle.iGender;
    finish.HairColor = character->PCStyle.iHairColor;
    finish.HairStyle = character->PCStyle.iHairStyle;
    finish.Height = character->PCStyle.iHeight;
    finish.Level = 1;
    finish.SkinColor = character->PCStyle.iSkinColor;
    db.update(finish);
}

void Database::finishTutorial(int PlayerID) 
{
    DbPlayer finish = getDbPlayerById(PlayerID);
    finish.TutorialFlag = 1;
    //equip lightning gun
    finish.EquipWeapon1 = 328;
    db.update(finish);
}

int Database::deleteCharacter(int characterID) 
{
    auto find =
        db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == characterID));
    int slot = find.front().slot;
    db.remove<DbPlayer>(find.front().PlayerID);
    return slot;
}

std::vector <Player> Database::getCharacters(int UserID) 
{
    std::vector<DbPlayer>characters =
        db.get_all<DbPlayer>(where
        (c(&DbPlayer::AccountID) == UserID));
    //parsing DbPlayer to Player
    std::vector<Player> result = std::vector<Player>();
    for (auto &character : characters) {
        Player toadd = DbToPlayer(character);        
        result.push_back(
            toadd
        );
    }
    return result;
}

void Database::evaluateCustomName(int characterID, CUSTOMNAME decision) {
    DbPlayer player = getDbPlayerById(characterID);
    player.NameCheck = (int)decision;
    db.update(player);
}

void Database::changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save) {
    DbPlayer Player = getDbPlayerById(save->iPCUID);
    Player.FirstName = U16toU8(save->szFirstName);
    Player.LastName = U16toU8(save->szLastName);
    if (settings::APPROVEALLNAMES || save->iFNCode)
        Player.NameCheck = 1;
    else
        Player.NameCheck = 0;
    db.update(Player);
}

Database::DbPlayer Database::playerToDb(Player player) 
{
    DbPlayer result;
    result.PlayerID = player.iID;
    result.AccountID = player.accountId;
    result.AppearanceFlag = player.PCStyle2.iAppearanceFlag;
    result.Body = player.PCStyle.iBody;
    result.Class = player.PCStyle.iClass;
    //equipment
    result.EyeColor = player.PCStyle.iEyeColor;
    result.FaceStyle = player.PCStyle.iFaceStyle;
    result.FirstName = U16toU8( player.PCStyle.szFirstName);
    result.FusionMatter = player.fusionmatter;
    result.Gender = player.PCStyle.iGender;
    result.HairColor = player.PCStyle.iHairColor;
    result.HairStyle = player.PCStyle.iHairStyle;
    result.Height = player.PCStyle.iHeight;
    result.HP = player.HP;
    result.isGM = player.IsGM;
    result.LastName = U16toU8(player.PCStyle.szLastName);
    result.Level = player.level;
    result.NameCheck = player.PCStyle.iNameCheck;
    result.PayZoneFlag = player.PCStyle2.iPayzoneFlag;
    result.PlayerID = player.PCStyle.iPC_UID;
    result.SkinColor = player.PCStyle.iSkinColor;
    result.slot = player.slot;
    result.Taros = player.money;
    result.TutorialFlag = player.PCStyle2.iTutorialFlag;
    result.x_coordinates = player.x;
    result.y_coordinates = player.y;
    result.z_coordinates = player.z;
    result.angle = player.angle;
    result.PCState = player.iPCState;

    //temporary inventory stuff
    result.EquipWeapon1 = player.Equip[0].iID;
    result.EquipUB = player.Equip[1].iID;
    result.EquipLB = player.Equip[2].iID;
    result.EquipFoot = player.Equip[3].iID;
        
    return result;
}

Player Database::DbToPlayer(DbPlayer player) {
    Player result;
    
    result.iID = player.PlayerID;
    result.accountId = player.AccountID;
    result.PCStyle2.iAppearanceFlag = player.AppearanceFlag;
    result.PCStyle.iBody = player.Body;
    result.PCStyle.iClass = player.Class;
    result.PCStyle.iEyeColor = player.EyeColor;
    result.PCStyle.iFaceStyle = player.FaceStyle;
    U8toU16(player.FirstName, result.PCStyle.szFirstName);
    result.PCStyle.iGender = player.Gender;
    result.PCStyle.iHairColor = player.HairColor;
    result.PCStyle.iHairStyle = player.HairStyle;
    result.PCStyle.iHeight = player.Height;
    result.HP = player.HP;
    result.IsGM = player.isGM;
    U8toU16(player.LastName, result.PCStyle.szLastName);
    result.level = player.Level;
    result.PCStyle.iNameCheck = player.NameCheck;
    result.PCStyle2.iPayzoneFlag = player.PayZoneFlag;
    result.iID = player.PlayerID;
    result.PCStyle.iPC_UID = player.PlayerID;
    result.PCStyle.iSkinColor = player.SkinColor;
    result.slot = player.slot;
    result.PCStyle2.iTutorialFlag = player.TutorialFlag;
    result.x = player.x_coordinates;
    result.y = player.y_coordinates;
    result.z = player.z_coordinates;
    result.angle = player.angle;
    result.fusionmatter = player.FusionMatter;
    result.money = player.Taros;
    result.iPCState = player.PCState;

    //junk
    result.SerialKey = 0;
    result.isTrading = false;
    result.isTradeConfirm = false;
    //TODO:: implement all of below
    
    //Nanos    
    result.activeNano = -1;
    result.equippedNanos[0] = 0;
    result.equippedNanos[1] = 0;
    result.equippedNanos[2] = 0;

    result.Nanos[0].iID = 0;
    result.Nanos[0].iSkillID = 0;
    result.Nanos[0].iStamina = 0;

    for (int i = 1; i < 37; i++) {
        result.Nanos[i].iID = i;
        result.Nanos[i].iSkillID = 1;
        result.Nanos[i].iStamina = 150;
    }
    
    //equip
    result.Equip[0].iID = player.EquipWeapon1;
    result.Equip[0].iType = 0;
    (player.EquipWeapon1) ? result.Equip[0].iOpt = 1 : result.Equip[0].iOpt = 0;

    result.Equip[1].iID = player.EquipUB;
    result.Equip[1].iType = 1;
    (player.EquipUB) ? result.Equip[1].iOpt = 1 : result.Equip[1].iOpt = 0;

    result.Equip[2].iID = player.EquipLB;
    result.Equip[2].iType = 2;
    (player.EquipLB) ? result.Equip[2].iOpt = 1 : result.Equip[2].iOpt = 0;

    result.Equip[3].iID = player.EquipFoot;
    result.Equip[3].iType = 3;
    (player.EquipFoot) ? result.Equip[3].iOpt = 1 : result.Equip[3].iOpt = 0;



    for (int i = 4; i < AEQUIP_COUNT; i++) {
        // empty equips
        result.Equip[i].iID = 0;
        result.Equip[i].iType = i;
        result.Equip[i].iOpt = 0;
    }
    for (int i = 0; i < AINVEN_COUNT; i++) {
        // setup inventories
        result.Inven[i].iID = 0;
        result.Inven[i].iType = 0;
        result.Inven[i].iOpt = 0;
    }
    return result;
}

Database::DbPlayer Database::getDbPlayerById(int id) {
        return db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == id))
            .front();
}

#pragma endregion LoginServer

void Database::updatePlayer(Player player) {
    DbPlayer toUpdate = playerToDb(player);
    db.update(toUpdate);
}
