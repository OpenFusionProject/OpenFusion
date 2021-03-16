#pragma once

#include "CNShardServer.hpp"
#include "Player.hpp"
#include "MobAI.hpp"

struct CrocPotEntry {
    int multStats, multLooks;
    float base, rd0, rd1, rd2, rd3;
};

struct Crate {
    int rarityRatioId;
    std::vector<int> itemSets;
};

struct MobDropChance {
    int dropChance;
    std::vector<int> cratesRatio;
};

struct MobDrop {
    std::vector<int> crateIDs;
    int dropChanceType;
    int taros;
    int fm;
    int boosts;
};

namespace ItemManager {
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
    extern std::map<int32_t, std::vector<int>> RarityRatios;
    extern std::map<int32_t, Crate> Crates;
    // pair <Itemset, Rarity> -> vector of pointers (map iterators) to records in ItemData (it looks a lot scarier than it is)
    extern std::map<std::pair<int32_t, int32_t>,
        std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator>> CrateItems;
    extern std::map<std::string, std::vector<std::pair<int32_t, int32_t>>> CodeItems; // code -> vector of <id, type>

    // mob drops
    extern std::map<int32_t, MobDropChance> MobDropChances;
    extern std::map<int32_t, MobDrop> MobDrops;

    void init();

    // crate opening logic with all helper functions
    int getItemSetId(Crate& crate, int crateId);
    int getRarity(Crate& crate, int itemSetId);
    int getCrateItem(sItemBase& reward, int itemSetId, int rarity, int playerGender);

    // mob drops
    void giveMobDrop(CNSocket *sock, Mob *mob, int rolledBoosts, int rolledPotions, int rolledCrate, int rolledCrateType, int rolledEvent);
    void getMobDrop(sItemBase *reward, MobDrop *drop, MobDropChance *chance, int rolled);
    void giveEventDrop(CNSocket* sock, Player* player, int rolled);

    int findFreeSlot(Player *plr);
    Item* getItemData(int32_t id, int32_t type);
    void checkItemExpire(CNSocket* sock, Player* player);
    void setItemStats(Player* plr);
    void updateEquips(CNSocket* sock, Player* plr);

#ifdef ACADEMY
    extern std::map<int32_t, int32_t> NanoCapsules; // crate id -> nano id
#endif
}
