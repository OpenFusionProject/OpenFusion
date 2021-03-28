#pragma once

#include "servers/CNShardServer.hpp"
#include "Player.hpp"
#include "MobAI.hpp"

struct CrocPotEntry {
    int multStats, multLooks;
    float base, rd0, rd1, rd2, rd3;
};

struct Crate {
    int itemSetChanceId;
    int itemSetTypeId;
    int rarityWeightId;
};

struct CrateDropChance {
    int dropChance, dropChanceTotal;
    std::vector<int> crateWeights;
};

struct MiscDropChance {
    int potionDropChance, potionDropChanceTotal;
    int boostDropChance, boostDropChanceTotal;
    int taroDropChance, taroDropChanceTotal;
    int fmDropChance, fmDropChanceTotal;
};

struct MiscDropType {
    int potionAmount;
    int boostAmount;
    int taroAmount;
    int fmAmount;
};

struct MobDrop {
    int crateDropChanceId;
    int crateDropTypeId;
    int miscDropChanceId;
    int miscDropTypeId;
};

struct ItemSetType {
    bool ignoreGender;
    std::vector<int> droppableItemIds;
};

struct ItemSetChance {
    int defaultItemWeight;
    std::map<int, int> specialItemWeights;
};

struct DroppableItem {
    int itemId;
    int rarity;
    int type;
};

namespace Items {
    enum class SlotType {
        EQUIP = 0,
        INVENTORY = 1,
        BANK = 3
    };
    struct Item {
        bool tradeable, sellable;
        int buyPrice, sellPrice;
        int stackSize, level, rarity;
        int pointDamage, groupDamage, fireRate, defense, gender;
        int weaponType;
        // TODO: implement more as needed
    };
    // hopefully this is fine since it's never modified after load
    extern std::map<std::pair<int32_t, int32_t>, Item> ItemData; // <id, type> -> data
    extern std::map<int32_t, CrocPotEntry> CrocPotTable; // level gap -> entry
    extern std::map<int32_t, std::vector<int32_t>> RarityWeights;
    extern std::map<int32_t, Crate> Crates;
    extern std::map<int32_t, DroppableItem> DroppableItems;
    extern std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> CodeItems; // code -> vector of <id, type>

    // mob drops
    extern std::map<int32_t, CrateDropChance> CrateDropChances;
    extern std::map<int32_t, std::vector<int32_t>> CrateDropTypes;
    extern std::map<int32_t, MiscDropChance> MiscDropChances;
    extern std::map<int32_t, MiscDropType> MiscDropTypes;
    extern std::map<int32_t, MobDrop> MobDrops;
    extern std::map<int32_t, ItemSetType> ItemSetTypes;
    extern std::map<int32_t, ItemSetChance> ItemSetChances;

    void init();

    // mob drops
    void giveMobDrop(CNSocket *sock, Mob *mob, int rolledBoosts, int rolledPotions, int rolledCrate, int rolledCrateType, int rolledEvent);

    int findFreeSlot(Player *plr);
    Item* getItemData(int32_t id, int32_t type);
    void checkItemExpire(CNSocket* sock, Player* player);
    void setItemStats(Player* plr);
    void updateEquips(CNSocket* sock, Player* plr);

#ifdef ACADEMY
    extern std::map<int32_t, int32_t> NanoCapsules; // crate id -> nano id
#endif
}
