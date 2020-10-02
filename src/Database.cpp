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

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

using namespace sqlite_orm;

std::mutex dbCrit;

# pragma region DatabaseScheme
auto db = make_storage("database.db",
    make_table("Accounts",
        make_column("AccountID", &Database::Account::AccountID, autoincrement(), primary_key()),
        make_column("Login", &Database::Account::Login),
        make_column("Password", &Database::Account::Password),
        make_column("Selected", &Database::Account::Selected),
        make_column("Created", &Database::Account::Created),
        make_column("LastLogin", &Database::Account::LastLogin)
    ),
    make_table("Players",
        make_column("PlayerID", &Database::DbPlayer::PlayerID, autoincrement(), primary_key()),
        make_column("AccountID", &Database::DbPlayer::AccountID),
        make_column("Slot", &Database::DbPlayer::slot),
        make_column("Firstname", &Database::DbPlayer::FirstName, collate_nocase()),
        make_column("LastName", &Database::DbPlayer::LastName, collate_nocase()),
        make_column("Created", &Database::DbPlayer::Created),
        make_column("LastLogin", &Database::DbPlayer::LastLogin),
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
        make_column("AccountLevel", &Database::DbPlayer::AccountLevel),
        make_column("FusionMatter", &Database::DbPlayer::FusionMatter),
        make_column("Taros", &Database::DbPlayer::Taros),
        make_column("Quests", &Database::DbPlayer::QuestFlag),
        make_column("BatteryW", &Database::DbPlayer::BatteryW),
        make_column("BatteryN", &Database::DbPlayer::BatteryN),
        make_column("Mentor", &Database::DbPlayer::Mentor),
        make_column("WarpLocationFlag", &Database::DbPlayer::WarpLocationFlag),
        make_column("SkywayLocationFlag1", &Database::DbPlayer::SkywayLocationFlag1),
        make_column("SkywayLocationFlag2", &Database::DbPlayer::SkywayLocationFlag2),
        make_column("CurrentMissionID", &Database::DbPlayer::CurrentMissionID)
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
    ),
    make_table("RunningQuests",
        make_column("PlayerId", &Database::DbQuest::PlayerId),
        make_column("TaskId", &Database::DbQuest::TaskId),
        make_column("RemainingNPCCount1", &Database::DbQuest::RemainingNPCCount1),
        make_column("RemainingNPCCount2", &Database::DbQuest::RemainingNPCCount2),
        make_column("RemainingNPCCount3", &Database::DbQuest::RemainingNPCCount3)
    )
);

# pragma endregion DatabaseScheme

#pragma region LoginServer

void Database::open()
{
    // this parameter means it will try to preserve data during migration
    bool preserve = true;
    db.sync_schema(preserve);
    std::cout << "[INFO] Database in operation ";
    int accounts = getAccountsCount();
    int players = getPlayersCount();
    std::string message = "";
    if (accounts > 0) {
        message += ": Found " + std::to_string(accounts) + " Account";
        if (accounts > 1)
            message += "s";
    }
    if (players > 0) {
        message += " and " + std::to_string(players) + " Player Character";
        if (players > 1)
            message += "s";
    }
    std::cout << message << std::endl;
}

int Database::getAccountsCount() {
    return db.count<Account>();
}

int Database::getPlayersCount() {
    return db.count<DbPlayer>();
}

int Database::addAccount(std::string login, std::string password)
{
    std::lock_guard<std::mutex> lock(dbCrit);

    password = BCrypt::generateHash(password);
    Account account = {};
    account.Login = login;
    account.Password = password;
    account.Selected = 1;
    account.Created = getTimestamp();
    return db.insert(account);
}

void Database::updateSelected(int accountId, int slot)
{
    std::lock_guard<std::mutex> lock(dbCrit);

    Account acc = db.get<Account>(accountId);
    acc.Selected = slot;
    //timestamp
    acc.LastLogin = getTimestamp();
    db.update(acc);
}

std::unique_ptr<Database::Account> Database::findAccount(std::string login)
{
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

    // fail if the player already has 4 or more characters
    if (db.count<DbPlayer>(where(c(&DbPlayer::AccountID) == AccountID)) >= 4)
        return -1;

    DbPlayer create = {};
    
    //set timestamp
    create.Created = getTimestamp();
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
    create.AccountLevel = settings::ACCLEVEL;
    create.x_coordinates = settings::SPAWN_X;
    create.y_coordinates = settings::SPAWN_Y;
    create.z_coordinates = settings::SPAWN_Z;
    create.angle = settings::SPAWN_ANGLE;
    //set mentor to computress
    create.Mentor = 5;

    // initialize the quest blob to 128 0-bytes
    create.QuestFlag = std::vector<char>(128, 0);

    return db.insert(create);
}

void Database::finishCharacter(sP_CL2LS_REQ_CHAR_CREATE* character)
{
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

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
    std::lock_guard<std::mutex> lock(dbCrit);

    DbPlayer player = getDbPlayerById(characterID);
    player.NameCheck = (int)decision;
    db.update(player);
}

void Database::changeName(sP_CL2LS_REQ_CHANGE_CHAR_NAME* save) {
    std::lock_guard<std::mutex> lock(dbCrit);

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
    //TODO: move stuff that is never updated to separate table so it doesn't try to update it every time
    DbPlayer result = {};

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
    result.AccountLevel = player->accountLevel;
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
    result.BatteryN = player->batteryN;
    result.BatteryW = player->batteryW;
    result.Mentor = player->mentor;
    result.WarpLocationFlag = player->iWarpLocationFlag;
    result.SkywayLocationFlag1 = player->aSkywayLocationFlag[0];
    result.SkywayLocationFlag2 = player->aSkywayLocationFlag[1];
    result.CurrentMissionID = player->CurrentMissionID;

    // timestamp
    result.LastLogin = getTimestamp();
    result.Created = player->creationTime;

    // save completed quests
    result.QuestFlag = std::vector<char>((char*)player->aQuestFlag, (char*)player->aQuestFlag + 128);

    return result;
}

Player Database::DbToPlayer(DbPlayer player) {
    Player result = {}; // fixes some weird memory errors, this zeros out the members (not the padding inbetween though)

    result.iID = player.PlayerID;
    result.accountId = player.AccountID;
    result.creationTime = player.Created;
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
    result.accountLevel = player.AccountLevel;
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
    result.batteryN = player.BatteryN;
    result.batteryW = player.BatteryW;
    result.mentor = player.Mentor;
    result.CurrentMissionID = player.CurrentMissionID;

    result.equippedNanos[0] = player.Nano1;
    result.equippedNanos[1] = player.Nano2;
    result.equippedNanos[2] = player.Nano3;

    result.dotDamage = false;
    result.inCombat = false;

    result.iWarpLocationFlag = player.WarpLocationFlag;
    result.aSkywayLocationFlag[0] = player.SkywayLocationFlag1;
    result.aSkywayLocationFlag[1] = player.SkywayLocationFlag2;

    Database::getInventory(&result);
    Database::removeExpiredVehicles(&result);
    Database::getNanos(&result);
    Database::getQuests(&result);   

    // load completed quests
    memcpy(&result.aQuestFlag, player.QuestFlag.data(), std::min(sizeof(result.aQuestFlag), player.QuestFlag.size()));

    return result;
}

Database::DbPlayer Database::getDbPlayerById(int id) {
    return db.get_all<DbPlayer>(where(c(&DbPlayer::PlayerID) == id)).front();
}

Player Database::getPlayer(int id) {
    return DbToPlayer(
        getDbPlayerById(id)
    );
}

#pragma endregion LoginServer

#pragma region ShardServer

void Database::updatePlayer(Player *player) {
    std::lock_guard<std::mutex> lock(dbCrit);

    DbPlayer toUpdate = playerToDb(player);
    db.update(toUpdate);
    updateInventory(player);
    updateNanos(player);
    updateQuests(player);
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
    // insert quest items
    for (int i = 0; i < AQINVEN_COUNT; i++) {
        if (player->QInven[i].iID != 0) {
            sItemBase* next = &player->QInven[i];
            Inventory toAdd = {};
            toAdd.playerId = player->iID;
            toAdd.slot = i + AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT;
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

void Database::updateQuests(Player* player) {
    // start transaction
    db.begin_transaction();
    // remove all
    db.remove_all<DbQuest>(
        where(c(&DbQuest::PlayerId) == player->iID)
        );
    // insert
    for (int i = 0; i < ACTIVE_MISSION_COUNT; i++)
    {
        if (player->tasks[i] == 0)
            continue;
        DbQuest toAdd = {};
        toAdd.PlayerId = player->iID;
        toAdd.TaskId = player->tasks[i];
        toAdd.RemainingNPCCount1 = player->RemainingNPCCount[i][0];
        toAdd.RemainingNPCCount2 = player->RemainingNPCCount[i][1];
        toAdd.RemainingNPCCount3 = player->RemainingNPCCount[i][2];
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
        if (current.slot < AEQUIP_COUNT)
            player->Equip[current.slot] = toSet;
        else if (current.slot < (AEQUIP_COUNT + AINVEN_COUNT))
            player->Inven[current.slot - AEQUIP_COUNT] = toSet;
        else if (current.slot < (AEQUIP_COUNT + AINVEN_COUNT + ABANK_COUNT))
            player->Bank[current.slot - AEQUIP_COUNT - AINVEN_COUNT] = toSet;
        else
            player->QInven[current.slot - AEQUIP_COUNT - AINVEN_COUNT - ABANK_COUNT] = toSet;
    }


}

void Database::removeExpiredVehicles(Player* player) {
    int32_t currentTime = getTimestamp();
    // remove from bank immediately
    for (int i = 0; i < ABANK_COUNT; i++) {
        if (player->Bank[i].iType == 10 && player->Bank[i].iTimeLimit < currentTime)
            player->Bank[i] = {};
    }
    // for the rest, we want to leave only 1 expired vehicle on player to delete it with the client packet
    std::vector<sItemBase*> toRemove;

    // equiped vehicle
    if (player->Equip[8].iOpt > 0 && player->Equip[8].iTimeLimit < currentTime)
    {
        toRemove.push_back(&player->Equip[8]);
        player->toRemoveVehicle.eIL = 0;
        player->toRemoveVehicle.iSlotNum = 8;
    }
    // inventory
    for (int i = 0; i < AINVEN_COUNT; i++) {
        if (player->Inven[i].iType == 10 && player->Inven[i].iTimeLimit < currentTime) {
            toRemove.push_back(&player->Inven[i]);
            player->toRemoveVehicle.eIL = 1;
            player->toRemoveVehicle.iSlotNum = i;
        }
    }

    // delete all but one vehicles, leave last one for ceremonial deletion
    for (int i = 0; i < (int)toRemove.size()-1; i++) {
        memset(toRemove[i], 0, sizeof(sItemBase));
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

void Database::getQuests(Player* player) {
    // get from DB
    auto quests = db.get_all<DbQuest>(
        where(c(&DbQuest::PlayerId) == player->iID)
        );
    // set
    int i = 0;
    for (const DbQuest& current : quests) {
        player->tasks[i] = current.TaskId;
        player->RemainingNPCCount[i][0] = current.RemainingNPCCount1;
        player->RemainingNPCCount[i][1] = current.RemainingNPCCount2;
        player->RemainingNPCCount[i][2] = current.RemainingNPCCount3;
        i++;
    }
}

#pragma endregion ShardServer
