#pragma once

#include "ItemManager.hpp"

struct VendorListing {
    int sort, type, iID;
};

namespace Vendor {
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;
}