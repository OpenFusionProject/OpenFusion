#pragma once

#include "CNProtocol.hpp"
#include "CNStructs.hpp"
#include "CNShardServer.hpp"

#include "ItemManager.hpp"
#include "PlayerManager.hpp"

struct VendorListing {
    int sort, type, iID;
};

namespace Vendor {
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;

    void init();

    void vendorStart(CNSocket* sock, CNPacketData* data);
    void vendorTable(CNSocket* sock, CNPacketData* data);
    void vendorBuy(CNSocket* sock, CNPacketData* data);
    void vendorSell(CNSocket* sock, CNPacketData* data);
    void vendorBuyback(CNSocket* sock, CNPacketData* data);
    void vendorBuyBattery(CNSocket* sock, CNPacketData* data);
}
