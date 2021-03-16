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
}
