#pragma once

#include "core/Core.hpp"
#include "servers/CNShardServer.hpp"

#include "Items.hpp"
#include "PlayerManager.hpp"

struct VendorListing {
    int sort, type, id;

    // when validating a listing, we don't really care about the sorting index
    bool operator==(const VendorListing& other) const {
        return type == other.type && id == other.id;
    }
};

namespace Vendors {
    extern std::map<int32_t, std::vector<VendorListing>> VendorTables;

    void init();
}
