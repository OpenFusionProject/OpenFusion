#pragma once

#include "CNShardServer.hpp"
#include "Player.hpp"

namespace ItemManager {
    enum class SlotType {
        EQUIP = 0,
        INVENTORY = 1,
        BANK = 3
    };
    void init();

    void itemMoveHandler(CNSocket* sock, CNPacketData* data);
    void itemDeleteHandler(CNSocket* sock, CNPacketData* data);
    void itemGMGiveHandler(CNSocket* sock, CNPacketData* data);
    // Bank
    void itemBankOpenHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferHandler(CNSocket* sock, CNPacketData* data);
    //void itemTradeOfferCancel(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeConfirmHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeRegisterItemHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeUnregisterItemHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeRegisterCashHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeChatHandler(CNSocket* sock, CNPacketData* data);
    void chestOpenHandler(CNSocket* sock, CNPacketData* data);

    int findFreeSlot(Player *plr);
}
