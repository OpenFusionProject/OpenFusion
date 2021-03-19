#pragma once

#include "CNStructs.hpp"

#include <map>

// Packet Descriptor
struct PacketDesc {
        uint32_t val;
        std::string name;
        size_t size;
        bool variadic;
        size_t cntMembOfs;
        size_t trailerSize;

        PacketDesc() {}

        PacketDesc(const PacketDesc& other) {
            val = other.val;
            name = other.name;
            size = other.size;
            variadic = other.variadic;
            cntMembOfs = other.cntMembOfs;
            trailerSize = other.trailerSize;
        }

        PacketDesc(PacketDesc&& other) {
            val = other.val;
            name = std::move(other.name);
            size = other.size;
            variadic = other.variadic;
            cntMembOfs = other.cntMembOfs;
            trailerSize = other.trailerSize;
        }

        // non-variadic constructor
        PacketDesc(uint32_t v, size_t s, std::string n) :
            val(v), name(n), size(s), variadic(false) {}

        // variadic constructor
        PacketDesc(uint32_t v, size_t s, std::string n, size_t ofs, size_t ts) :
            val(v), name(n), size(s), variadic(true), cntMembOfs(ofs), trailerSize(ts) {}
};

/*
 * Extra trailer structs for places where the client doesn't have any, but
 * really should.
 */
struct sGM_PVPTarget {
    uint32_t eCT;
    uint32_t iID;
};

struct sSkillResult_Leech {
    sSkillResult_Heal_HP Heal;
    sSkillResult_Damage Damage;
};

namespace Packets {
    extern std::map<uint32_t, PacketDesc> packets;

    std::string p2str(int val);
}
