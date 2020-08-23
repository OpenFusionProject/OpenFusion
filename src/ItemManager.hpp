#ifndef _IM_HPP
#define _IM_HPP

#include "CNShardServer.hpp"

namespace ItemManager {
    void init();	
    void itemMoveHandler(CNSocket* sock, CNPacketData* data);   
    void itemDeleteHandler(CNSocket* sock, CNPacketData* data);   
    void itemGMGiveHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferHandler(CNSocket* sock, CNPacketData* data);
    //void itemTradeOfferCancel(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferAcceptHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeOfferRefusalHandler(CNSocket* sock, CNPacketData* data);
    void itemTradeConfirmCancelHandler(CNSocket* sock, CNPacketData* data);
}

#endif