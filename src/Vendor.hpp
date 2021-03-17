#pragma once

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"

#include "Items.hpp"
#include "PlayerManager.hpp"

struct VendorListing {
    int sort, type, iID;
};

namespace Vendor {
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;

    void init();
}
