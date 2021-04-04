#pragma once

#include "servers/CNShardServer.hpp"
#include "Player.hpp"
#include "MobAI.hpp"
#include "Rand.hpp"

struct CrocPotEntry {
    int multStats, multLooks;
    float base, rd0, rd1, rd2, rd3;
};

struct Crate {
    int itemSetId;
    int rarityWeightId;
};

struct CrateDropChance {
    int dropChance, dropChanceTotal;
    std::vector<int> crateTypeDropWeights;
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

struct ItemSet {
    // itemset-wise offswitch to rarity filtering, every crate drops every rarity (still based on rarity weights)
    bool ignoreRarity;
    // itemset-wise offswitch for gender filtering, every crate can now drop neutral/boys/girls items
    bool ignoreGender;
    // default weight of all items in the itemset
    int defaultItemWeight;
    // change the rarity class of items in the itemset here
    // rarity 0 bypasses the rarity filter for an individual item
    std::map<int, int> alterRarityMap;
    // change the gender class of items in the itemset here
    // gender 0 bypasses the gender filter for an individual item
    std::map<int, int> alterGenderMap;
    // change the item weghts items in the itemset here
    // only taken into account for chosen rarity, and if the item isn't filtered away due to gender
    std::map<int, int> alterItemWeightMap;
    std::vector<int> itemReferenceIds;
};

struct ItemReference {
    int itemId;
    int type;
    int rarity;
    int gender;
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
    struct DropRoll {
        int boosts, potions;
        int taros, fm;
        int crate, crateType;

        DropRoll() : boosts(Rand::rand()), potions(Rand::rand()), taros(Rand::rand()), fm(Rand::rand()), crate(Rand::rand()), crateType(Rand::rand()) { }
    };
    // hopefully this is fine since it's never modified after load
    extern std::map<std::pair<int32_t, int32_t>, Item> ItemData; // <id, type> -> data
    extern std::map<int32_t, CrocPotEntry> CrocPotTable; // level gap -> entry
    extern std::map<int32_t, std::vector<int32_t>> RarityWeights;
    extern std::map<int32_t, Crate> Crates;
    extern std::map<int32_t, ItemReference> ItemReferences;
    extern std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> CodeItems; // code -> vector of <id, type>

    // mob drops
    extern std::map<int32_t, CrateDropChance> CrateDropChances;
    extern std::map<int32_t, std::vector<int32_t>> CrateDropTypes;
    extern std::map<int32_t, MiscDropChance> MiscDropChances;
    extern std::map<int32_t, MiscDropType> MiscDropTypes;
    extern std::map<int32_t, MobDrop> MobDrops;
    extern std::map<int32_t, int32_t> EventToDropMap;
    extern std::map<int32_t, int32_t> MobToDropMap;
    extern std::map<int32_t, ItemSet> ItemSets;

    void init();

    // mob drops
    void giveMobDrop(CNSocket *sock, Mob *mob, const DropRoll& rolled, const DropRoll& eventRolled);

    int findFreeSlot(Player *plr);
    Item* getItemData(int32_t id, int32_t type);
    void checkItemExpire(CNSocket* sock, Player* player);
    void setItemStats(Player* plr);
    void updateEquips(CNSocket* sock, Player* plr);

#ifdef ACADEMY
    extern std::map<int32_t, int32_t> NanoCapsules; // crate id -> nano id
#endif
}
