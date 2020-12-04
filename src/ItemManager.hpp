#pragma once

#include "CNShardServer.hpp"
#include "Player.hpp"

struct VendorListing {
    int sort, type, iID;
};
struct CrocPotEntry {
    int multStats, multLooks;
    float base, rd0, rd1, rd2, rd3;
};
struct Crate {
    int rarityRatioId;
    std::vector<int> itemSets;
};

namespace ItemManager {
    enum class SlotType {
        EQUIP = 0,
        INVENTORY = 1,
        BANK = 3
    };
    struct Item {
        bool tradeable, sellable;
        int buyPrice, sellPrice, stackSize, level, rarity, pointDamage, groupDamage, defense, gender; // TODO: implement more as needed
    };
    // hopefully this is fine since it's never modified after load
    extern std::map<std::pair<int32_t, int32_t>, Item> ItemData; // <id, type> -> data
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;
    extern std::map<int32_t, CrocPotEntry> CrocPotTable; // level gap -> entry
    extern std::map<int32_t, std::vector<int>> RarityRatios;
    extern std::map<int32_t, Crate> Crates;
    // pair <Itemset, Rarity> -> vector of pointers (map iterators) to records in ItemData (it looks a lot scarier than it is)
    extern std::map<std::pair<int32_t, int32_t>,
        std::vector<std::map<std::pair<int32_t, int32_t>, Item>::iterator>> CrateItems;

    void init();

    void itemMoveHandler(CNSocket* sock, CNPacketData* data);
    void itemDeleteHandler(CNSocket* sock, CNPacketData* data);
    void itemGMGiveHandler(CNSocket* sock, CNPacketData* data);
    void itemUseHandler(CNSocket* sock, CNPacketData* data);
    // Bank
    void itemBankOpenHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeConfirmHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeRegisterItemHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeUnregisterItemHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeRegisterCashHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeChatHandler(CNSocket* sock, CNPacketData* data);
    void chestOpenHandler(CNSocket* sock, CNPacketData* data);

    // crate opening logic with all helper functions
    int getItemSetId(Crate& crate, int crateId);
    int getRarity(Crate& crate, int itemSetId);
    int getCrateItem(sItemBase& reward, int itemSetId, int rarity, int playerGender);

    int findFreeSlot(Player *plr);
    Item* getItemData(int32_t id, int32_t type);
    void checkItemExpire(CNSocket* sock, Player* player);
    void setItemStats(Player* plr);
    void updateEquips(CNSocket* sock, Player* plr);
}
