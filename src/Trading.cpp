#include "Trading.hpp"
#include "PlayerManager.hpp"

using namespace Trading;

static bool doTrade(Player* plr, Player* plr2) {
    // init dummy inventories
    sItemBase plrInven[AINVEN_COUNT];
    sItemBase plr2Inven[AINVEN_COUNT];
    memcpy(plrInven, plr->Inven, AINVEN_COUNT * sizeof(sItemBase));
    memcpy(plr2Inven, plr2->Inven, AINVEN_COUNT * sizeof(sItemBase));

    for (int i = 0; i < 5; i++) {
        // remove items offered by us
        if (plr->Trade[i].iID != 0) {
            if (plrInven[plr->Trade[i].iInvenNum].iID == 0
                || plr->Trade[i].iID != plrInven[plr->Trade[i].iInvenNum].iID
                || plr->Trade[i].iType != plrInven[plr->Trade[i].iInvenNum].iType) // pulling a fast one on us
                return false;

            if (plr->Trade[i].iOpt < 1) {
                std::cout << "[WARN] Player tried trading an iOpt < 1 amount" << std::endl;
                plr->Trade[i].iOpt = 1;
            }

            // for stacked items
            plrInven[plr->Trade[i].iInvenNum].iOpt -= plr->Trade[i].iOpt;

            if (plrInven[plr->Trade[i].iInvenNum].iOpt == 0) {
                plrInven[plr->Trade[i].iInvenNum].iID = 0;
                plrInven[plr->Trade[i].iInvenNum].iType = 0;
                plrInven[plr->Trade[i].iInvenNum].iOpt = 0;
            } else if (plrInven[plr->Trade[i].iInvenNum].iOpt < 0) { // another dupe attempt
                return false;
            }
        }

        if (plr2->Trade[i].iID != 0) {
            if (plr2Inven[plr2->Trade[i].iInvenNum].iID == 0
                || plr2->Trade[i].iID != plr2Inven[plr2->Trade[i].iInvenNum].iID
                || plr2->Trade[i].iType != plr2Inven[plr2->Trade[i].iInvenNum].iType) // pulling a fast one on us
                return false;

            if (plr2->Trade[i].iOpt < 1) {
                std::cout << "[WARN] Player tried trading an iOpt < 1 amount" << std::endl;
                plr2->Trade[i].iOpt = 1;
            }

            // for stacked items
            plr2Inven[plr2->Trade[i].iInvenNum].iOpt -= plr2->Trade[i].iOpt;

            if (plr2Inven[plr2->Trade[i].iInvenNum].iOpt == 0) {
                plr2Inven[plr2->Trade[i].iInvenNum].iID = 0;
                plr2Inven[plr2->Trade[i].iInvenNum].iType = 0;
                plr2Inven[plr2->Trade[i].iInvenNum].iOpt = 0;
            } else if (plr2Inven[plr2->Trade[i].iInvenNum].iOpt < 0) { // another dupe attempt
                return false;
            }
        }

        // add items offered to us
        if (plr2->Trade[i].iID != 0) {
            for (int n = 0; n < AINVEN_COUNT; n++) {
                if (plrInven[n].iID == 0) {
                    plrInven[n].iID = plr2->Trade[i].iID;
                    plrInven[n].iType = plr2->Trade[i].iType;
                    plrInven[n].iOpt = plr2->Trade[i].iOpt;
                    plr2->Trade[i].iInvenNum = n;
                    break;
                }

                if (n >= AINVEN_COUNT - 1)
                    return false; // not enough space
            }
        }

        if (plr->Trade[i].iID != 0) {
            for (int n = 0; n < AINVEN_COUNT; n++) {
                if (plr2Inven[n].iID == 0) {
                    plr2Inven[n].iID = plr->Trade[i].iID;
                    plr2Inven[n].iType = plr->Trade[i].iType;
                    plr2Inven[n].iOpt = plr->Trade[i].iOpt;
                    plr->Trade[i].iInvenNum = n;
                    break;
                }

                if (n >= AINVEN_COUNT - 1)
                    return false; // not enough space
            }
        }
    }

    // if everything went well, back into player inventory it goes
    memcpy(plr->Inven, plrInven, AINVEN_COUNT * sizeof(sItemBase));
    memcpy(plr2->Inven, plr2Inven, AINVEN_COUNT * sizeof(sItemBase));

    return true;
}

static void tradeOffer(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_OFFER* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_To);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(otherSock);

    if (plr->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
        resp.iID_Request = pacdat->iID_To;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        return; // prevent trading with a player already trading
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER));
}

static void tradeOfferAccept(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    if (plr2->isTrading) {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
        resp.iID_Request = pacdat->iID_From;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
        return; // prevent trading with a player already trading
    }

    // clearing up trade slots
    plr->moneyInTrade = 0;
    plr2->moneyInTrade = 0;
    memset(&plr->Trade, 0, sizeof(plr->Trade));
    memset(&plr2->Trade, 0, sizeof(plr2->Trade));

    // marking players as traders
    plr->isTrading = true;
    plr2->isTrading = true;

    // marking players as unconfirmed
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    // inform the other player that offer is accepted
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_SUCC));
}

static void tradeOfferRefusal(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL* pacdat = (sP_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL*)data->buf;

    CNSocket* otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_OFFER_REFUSAL, sizeof(sP_FE2CL_REP_PC_TRADE_OFFER_REFUSAL));
}

static void tradeConfirm(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_CONFIRM* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM*)data->buf;

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);
    
    if (!(plr->isTrading && plr2->isTrading)) { // both players must be trading
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);
        resp.iID_Request = plr2->iID;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        resp.iID_Request = plr->iID;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));

        // both players are no longer trading
        plr->isTrading = false;
        plr2->isTrading = false;
        plr->isTradeConfirm = false;
        plr2->isTradeConfirm = false;

        return;
    }

    // send the confirm packet
    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));

    if (!(plr2->isTradeConfirm)) {
        plr->isTradeConfirm = true;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM));
        return;
    }

    // both players are no longer trading
    plr->isTrading = false;
    plr2->isTrading = false;
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    if (doTrade(plr, plr2)) { // returns false if not enough slots
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, resp2);
        resp2.iID_Request = pacdat->iID_Request;
        resp2.iID_From = pacdat->iID_From;
        resp2.iID_To = pacdat->iID_To;
        plr->money = plr->money + plr2->moneyInTrade - plr->moneyInTrade;
        resp2.iCandy = plr->money;
        memcpy(resp2.Item, plr2->Trade, sizeof(plr2->Trade));
        memcpy(resp2.ItemStay, plr->Trade, sizeof(plr->Trade));

        sock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));

        plr2->money = plr2->money + plr->moneyInTrade - plr2->moneyInTrade;
        resp2.iCandy = plr2->money;
        memcpy(resp2.Item, plr->Trade, sizeof(plr->Trade));
        memcpy(resp2.ItemStay, plr2->Trade, sizeof(plr2->Trade));

        otherSock->sendPacket((void*)&resp2, P_FE2CL_REP_PC_TRADE_CONFIRM_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_SUCC));
    } else {
        INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, resp);
        resp.iID_Request = plr->iID;
        resp.iID_From = pacdat->iID_From;
        resp.iID_To = pacdat->iID_To;
        sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));
        resp.iID_Request = plr2->iID;
        otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_ABORT, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_ABORT));

        INITSTRUCT(sP_FE2CL_GM_REP_PC_ANNOUNCE, msg);
        std::string text = "Trade Failed";
        U8toU16(text, msg.szAnnounceMsg, sizeof(msg.szAnnounceMsg));
        msg.iDuringTime = 3;
        sock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        otherSock->sendPacket(msg, P_FE2CL_GM_REP_PC_ANNOUNCE);
        return;
    }
}

static void tradeConfirmCancel(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL* pacdat = (sP_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL*)data->buf;

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);

    // both players are not trading nor are in a confirmed state
    plr->isTrading = false;
    plr->isTradeConfirm = false;
    plr2->isTrading = false;
    plr2->isTradeConfirm = false;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL, sizeof(sP_FE2CL_REP_PC_TRADE_CONFIRM_CANCEL));
}

static void tradeRegisterItem(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_REGISTER*)data->buf;

    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check, there are only 5 trade slots

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);
    plr->Trade[pacdat->Item.iSlotNum] = pacdat->Item;
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    // since you can spread items like gumballs over multiple slots, we need to count them all
    // to make sure the inventory shows the right value during trade.
    int count = 0; 
    for (int i = 0; i < 5; i++) {
        if (plr->Trade[i].iInvenNum == pacdat->Item.iInvenNum)
            count += plr->Trade[i].iOpt;
    }

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = pacdat->Item;
    resp.InvenItem.iOpt = plr->Inven[pacdat->Item.iInvenNum].iOpt - count; // subtract this count

    if (resp.InvenItem.iOpt < 0) // negative count items, doTrade() will block this later on
        std::cout << "[WARN] tradeRegisterItem: an item went negative count client side." << std::endl;

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_REGISTER_SUCC));
}

static void tradeUnregisterItem(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER*)data->buf;

    if (pacdat->Item.iSlotNum < 0 || pacdat->Item.iSlotNum > 4)
        return; // sanity check, there are only 5 trade slots

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr = PlayerManager::getPlayer(sock);
    Player* plr2 = PlayerManager::getPlayer(otherSock);
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.TradeItem = pacdat->Item;
    resp.InvenItem = plr->Trade[pacdat->Item.iSlotNum];

    memset(&plr->Trade[pacdat->Item.iSlotNum], 0, sizeof(plr->Trade[pacdat->Item.iSlotNum])); // clean up item slot

    // since you can spread items like gumballs over multiple slots, we need to count them all
    // to make sure the inventory shows the right value during trade.
    int count = 0; 
    for (int i = 0; i < 5; i++) {
        if (plr->Trade[i].iInvenNum == resp.InvenItem.iInvenNum)
            count += plr->Trade[i].iOpt;
    }

    resp.InvenItem.iOpt = plr->Inven[resp.InvenItem.iInvenNum].iOpt - count; // subtract this count

    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_ITEM_UNREGISTER_SUCC));
}

static void tradeRegisterCash(CNSocket* sock, CNPacketData* data) {
    sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER* pacdat = (sP_CL2FE_REQ_PC_TRADE_CASH_REGISTER*)data->buf;

    Player* plr = PlayerManager::getPlayer(sock);

    if (pacdat->iCandy < 0 || pacdat->iCandy > plr->money)
       return; // famous glitch, begone

    CNSocket* otherSock; // weird flip flop because we need to know who the other player is
    if (pacdat->iID_Request == pacdat->iID_From)
        otherSock = PlayerManager::getSockFromID(pacdat->iID_To);
    else
        otherSock = PlayerManager::getSockFromID(pacdat->iID_From);

    if (otherSock == nullptr)
        return;

    Player* plr2 = PlayerManager::getPlayer(otherSock);
    plr->isTradeConfirm = false;
    plr2->isTradeConfirm = false;

    INITSTRUCT(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, resp);
    resp.iID_Request = pacdat->iID_Request;
    resp.iID_From = pacdat->iID_From;
    resp.iID_To = pacdat->iID_To;
    resp.iCandy = pacdat->iCandy;
    sock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));
    otherSock->sendPacket((void*)&resp, P_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC, sizeof(sP_FE2CL_REP_PC_TRADE_CASH_REGISTER_SUCC));

    plr->moneyInTrade = pacdat->iCandy;
    plr->isTradeConfirm = false;
}

void Trading::init() {
    // Trade handlers
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER, tradeOffer);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_ACCEPT, tradeOfferAccept);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_OFFER_REFUSAL, tradeOfferRefusal);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM, tradeConfirm);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CONFIRM_CANCEL, tradeConfirmCancel);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_REGISTER, tradeRegisterItem);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_ITEM_UNREGISTER, tradeUnregisterItem);
    REGISTER_SHARD_PACKET(P_CL2FE_REQ_PC_TRADE_CASH_REGISTER, tradeRegisterCash);
}
