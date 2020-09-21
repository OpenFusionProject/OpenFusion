#include "Database.hpp"
#include "contrib/bcrypt/BCrypt.hpp"
#include "CNProtocol.hpp"
#include <string>
#include "contrib/JSON.hpp"
#include "CNStructs.hpp"
#include "settings.hpp"
#include "Player.hpp"
#include "CNStructs.hpp"
#include "contrib/sqlite/sqlite_orm.h"
#include "MissionManager.hpp"

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
        make_column("Firstname", &Database::DbPlayer::FirstName, collate_nocase()),
        make_column("LastName", &Database::DbPlayer::LastName, collate_nocase()),
        make_column("Level", &Database::DbPlayer::Level),
        make_column("Nano1", &Database::DbPlayer::Nano1),
        make_column("Nano2", &Database::DbPlayer::Nano2),
        make_column("Nano3", &Database::DbPlayer::Nano3),
        make_column("AppearanceFlag", &Database::DbPlayer::AppearanceFlag),
        make_column("TutorialFlag", &Database::DbPlayer::TutorialFlag),
        make_column("PayZoneFlag", &Database::DbPlayer::PayZoneFlag),
        make_column("XCoordinates", &Database::DbPlayer::x_coordinates),
        make_column("YCoordinates", &Database::DbPlayer::y_coordinates),
        make_column("ZCoordinates", &Database::DbPlayer::z_coordinates),
        make_column("Angle", &Database::DbPlayer::angle),
        make_column("Body", &Database::DbPlayer::Body),
        make_column("Class", &Database::DbPlayer::Class),
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
        make_column("Quests", &Database::DbPlayer::QuestFlag)
    ),
    make_table("Inventory",
        make_column("PlayerId", &Database::Inventory::playerId),
        make_column("Slot", &Database::Inventory::slot),
        make_column("Id", &Database::Inventory::id),
        make_column("Type", &Database::Inventory::Type),
        make_column("Opt", &Database::Inventory::Opt),
        make_column("TimeLimit", &Database::Inventory::TimeLimit)
    ),
    make_table("Nanos",
        make_column("PlayerId", &Database::Nano::playerId),
        make_column("Id", &Database::Nano::iID),
        make_column("Skill", &Database::Nano::iSkillID),
        make_column("Stamina", &Database::Nano::iStamina)
    )
);

# pragma endregion DatabaseScheme

#pragma region LoginServer

void Database::open()
{
    // this parameter means it will try to preserve data during migration
    bool preserve = true;
    db.sync_schema(preserve);
    DEBUGLOG(
        std::cout << "[DB] Database in operation" << std::endl;
    )
}

int Database::addAccount(std::string login, std::string password)
{
    password = BCrypt::generateHash(password);
    Account x = {};
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
    // this is awful, I've tried everything to improve it
    auto find = db.get_all<Account>(
        where(c(&Account::Login) == login), limit(1));
    if (find.empty())
        return nullptr;
    return
        std::unique_ptr<Account>(new Account(find.front()));
}

bool Database::isNameFree(sP_CL2LS_REQ_CHECK_CHAR_NAME* nameCheck)
{
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
    // fail if the player already has 4 or more characters
    if (db.count<DbPlayer>(where(c(&DbPlayer::AccountID) == AccountID)) >= 4)
        return -1;

    DbPlayer create = {};

    // save packet data
    create.FirstName = U16toU8(save->szFirstName);
    create.LastName = U16toU8(save->szLastName);
    create.slot = save->iSlotNum;
    create.AccountID = AccountID;

    // set flags
    create.AppearanceFlag = 0;
    create.TutorialFlag = 0;
    create.PayZoneFlag = 0;

    // set namecheck based on setting
    if (settings::APPROVEALLNAMES || save->iFNCode)
        create.NameCheck = 1;
    else
        create.NameCheck = 0;

    // create default body character
    create.Body = 0;
    create.Class = 0;
    create.EyeColor = 1;
    create.FaceStyle = 1;
    create.Gender = 1;
    create.Level = 1;
    create.HP = PC_MAXHEALTH(create.Level);
    create.HairColor = 1;
    create.HairStyle = 1;
    create.Height = 0;
    create.SkinColor = 1;
    create.isGM = settings::GM;
    create.x_coordinates = settings::SPAWN_X;
    create.y_coordinates = settings::SPAWN_Y;
    create.z_coordinates = settings::SPAWN_Z;
    create.angle = settings::SPAWN_ANGLE;
    create.QuestFlag = std::vector<char>();

    return db.insert(create);
}

void Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character)
{
    DbPlayer finish = getDbPlayerById(character->PCStyle.iPC_UID);
    finish.AppearanceFlag = 1;
    finish.Body = character->PCStyle.iBody;
    finish.Class = character->PCStyle.iClass;
    finish.EyeColor = character->PCStyle.iEyeColor;
    finish.FaceStyle = character->PCStyle.iFaceStyle;
    finish.Gender = character->PCStyle.iGender;
    finish.HairColor = character->PCStyle.iHairColor;
    finish.HairStyle = character->PCStyle.iHairStyle;
    finish.Height = character->PCStyle.iHeight;
    finish.Level = 1;
    finish.SkinColor = character->PCStyle.iSkinColor;
    db.update(finish);
    // clothes
    Inventory Foot, LB, UB;
    Foot.playerId = character->PCStyle.iPC_UID;
    Foot.id = character->sOn_Item.iEquipFootID;
    Foot.Type = 3;
    Foot.slot = 3;
    Foot.Opt = 1;
    Foot.TimeLimit = 0;
    db.insert(Foot);
    LB.playerId = character->PCStyle.iPC_UID;
    LB.id = character->sOn_Item.iEquipLBID;
    LB.Type = 2;
    LB.slot = 2;
    LB.Opt = 1;
    LB.TimeLimit = 0;
    db.insert(LB);
    UB.playerId = character->PCStyle.iPC_UID;
    UB.id = character->sOn_Item.iEquipUBID;
    UB.Type = 1;
    UB.slot = 1;
    UB.Opt = 1;
    UB.TimeLimit = 0;
    db.insert(UB);
}

void Database::finishTutorial(int PlayerID)
{
    Player finish = getPlayer(PlayerID);
    // set flag
    finish.PCStyle2.iTutorialFlag= 1;
    // add Gun
    Inventory LightningGun = {};
    LightningGun.playerId = PlayerID;
    LightningGun.id = 328;
    LightningGun.slot = 0;
    LightningGun.Type = 0;
    LightningGun.Opt = 1;
    db.insert(LightningGun);
    // add Nano
    Nano Buttercup = {};
    Buttercup.playerId = PlayerID;
    Buttercup.iID = 1;
    Buttercup.iSkillID = 1;
    Buttercup.iStamina = 150;
    finish.equippedNanos[0] = 1;
    db.insert(Buttercup);
    // save missions
    MissionManager::saveMission(&finish, 0);
    MissionManager::saveMission(&finish, 1);

    db.update(playerToDb(&finish));
}

int Database::deleteCharacter(int characterID, int userID)
{
    auto find =
        db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == characterID and c(&DbPlayer::AccountID)==userID));
    int slot = find.front().slot;
    db.remove<DbPlayer>(find.front().PlayerID);
    db.remove_all<Inventory>(where(c(&Inventory::playerId) == characterID));
    db.remove_all<Nano>(where(c(&Nano::playerId) == characterID));

    return slot;
}

std::vector <Player> Database::getCharacters(int UserID)
{
    std::vector<DbPlayer>characters =
        db.get_all<DbPlayer>(where
        (c(&DbPlayer::AccountID) == UserID));
    // parsing DbPlayer to Player
    std::vector<Player> result = std::vector<Player>();
    for (auto &character : characters) {
        Player toadd = DbToPlayer(character);
        result.push_back(
            toadd
        );
    }
    return result;
}

// XXX: This is never called?
void Database::evaluateCustomName(int characterID, CustomName decision) {
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

Database::DbPlayer Database::playerToDb(Player *player)
{
    DbPlayer result = {};  // fixes some weird memory errors, this zeros out the members (not the padding inbetween though)

    result.PlayerID = player->iID;
    result.AccountID = player->accountId;
    result.AppearanceFlag = player->PCStyle2.iAppearanceFlag;
    result.Body = player->PCStyle.iBody;
    result.Class = player->PCStyle.iClass;
    result.EyeColor = player->PCStyle.iEyeColor;
    result.FaceStyle = player->PCStyle.iFaceStyle;
    result.FirstName = U16toU8( player->PCStyle.szFirstName);
    result.FusionMatter = player->fusionmatter;
    result.Gender = player->PCStyle.iGender;
    result.HairColor = player->PCStyle.iHairColor;
    result.HairStyle = player->PCStyle.iHairStyle;
    result.Height = player->PCStyle.iHeight;
    result.HP = player->HP;
    result.isGM = player->IsGM;
    result.LastName = U16toU8(player->PCStyle.szLastName);
    result.Level = player->level;
    result.NameCheck = player->PCStyle.iNameCheck;
    result.PayZoneFlag = player->PCStyle2.iPayzoneFlag;
    result.PlayerID = player->PCStyle.iPC_UID;
    result.SkinColor = player->PCStyle.iSkinColor;
    result.slot = player->slot;
    result.Taros = player->money;
    result.TutorialFlag = player->PCStyle2.iTutorialFlag;
    result.x_coordinates = player->x;
    result.y_coordinates = player->y;
    result.z_coordinates = player->z;
    result.angle = player->angle;
    result.Nano1 = player->equippedNanos[0];
    result.Nano2 = player->equippedNanos[1];
    result.Nano3 = player->equippedNanos[2];

    // quests
    result.QuestFlag = std::vector<char>();
    // parsing long array to char vector
    for (int i=0; i<16; i++)
    {
        int64_t temp = player->aQuestFlag[i];
        for (int j = 0; j < 8; j++) {
            int64_t check2 = (temp >> (8 * (7 - j)));
            char toadd = check2;
            result.QuestFlag.push_back(
                toadd
            );
        }
    }


    return result;
}

Player Database::DbToPlayer(DbPlayer player) {
    Player result = {}; // fixes some weird memory errors, this zeros out the members (not the padding inbetween though)

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
    result.money = player.Taros;
    result.fusionmatter = player.FusionMatter;

    result.equippedNanos[0] = player.Nano1;
    result.equippedNanos[1] = player.Nano2;
    result.equippedNanos[2] = player.Nano3;

    Database::getInventory(&result);
    Database::getNanos(&result);

    std::vector<char>::iterator it = player.QuestFlag.begin();
    for (int i = 0; i < 16; i++)
    {
        if (it == player.QuestFlag.end())
            break;

        int64_t toAdd = 0;
        for (int j = 0; j < 8; j++) {
            int64_t temp = *it;
            int64_t check2 = (temp << (8 * (7 - j)));
            toAdd += check2;
            it++;
        }
        result.aQuestFlag[i] = toAdd;
    }

    return result;
}

Database::DbPlayer Database::getDbPlayerById(int id) {
        return db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == id))
            .front();
}

Player Database::getPlayer(int id) {
    return DbToPlayer(
        getDbPlayerById(id)
    );
}

#pragma endregion LoginServer

#pragma region ShardServer

void Database::updatePlayer(Player *player) {
    DbPlayer toUpdate = playerToDb(player);
    db.update(toUpdate);
    updateInventory(player);
    updateNanos(player);
}

void Database::updateInventory(Player *player){
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<Inventory>(
        where(c(&Inventory::playerId) == player->iID)
        );
    // insert equip
    for (int i = 0; i < AEQUIP_COUNT; i++) {
        if (player->Equip[i].iID != 0) {
            sItemBase* next = &player->Equip[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    // insert inventory
    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iID != 0) {
            sItemBase* next = &player->Inven[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    // insert bank
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iID != 0) {
            sItemBase* next = &player->Bank[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT + AINVEN_COUNT;
            toAdd.id = next->iID;
            toAdd.Opt = next->iOpt;
            toAdd.Type = next->iType;
            toAdd.TimeLimit = next->iTimeLimit;
            db.insert(toAdd);
        }
    }
    db.commit();
}
void Database::updateNanos(Player *player) {
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<Nano>(
        where(c(&Nano::playerId) == player->iID)
        );
    // insert
    for (int i=1; i < SIZEOF_NANO_BANK_SLOT; i++)
    {
        if ((player->Nanos[i]).iID == 0)
            continue;
        Nano toAdd = {};
        sNano* next = &player->Nanos[i];
        toAdd.playerId = player->iID;
        toAdd.iID = next->iID;
        toAdd.iSkillID = next->iSkillID;
        toAdd.iStamina = next->iStamina;
        db.insert(toAdd);
    }
    db.commit();
}
void Database::getInventory(Player* player) {
    // get items from DB
    auto items = db.get_all<Inventory>(
        where(c(&Inventory::playerId) == player->iID)
        );
    // set items
    for (const Inventory &current : items) {
        sItemBase toSet = {};
        toSet.iID = current.id;
        toSet.iType = current.Type;
        toSet.iOpt = current.Opt;
        toSet.iTimeLimit = current.TimeLimit;
        // assign to proper arrays
        if (current.slot <= AEQUIP_COUNT)
            player->Equip[current.slot] = toSet;
        else if (current.slot <= (AEQUIP_COUNT + AINVEN_COUNT))
            player->Inven[current.slot - AEQUIP_COUNT] = toSet;
        else
            player->Bank[current.slot - AEQUIP_COUNT - AINVEN_COUNT] = toSet;
    }

}
void Database::getNanos(Player* player) {
    // get from DB
    auto nanos = db.get_all<Nano>(
        where(c(&Nano::playerId) == player->iID)
        );
    // set
    for (const Nano& current : nanos) {
        sNano *toSet = &player->Nanos[current.iID];
        toSet->iID = current.iID;
        toSet->iSkillID = current.iSkillID;
        toSet->iStamina = current.iStamina;
    }
}
#pragma endregion ShardServer
