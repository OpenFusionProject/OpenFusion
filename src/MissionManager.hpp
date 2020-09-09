#pragma once

#include "CNShardServer.hpp"

#include "contrib/JSON.hpp"

struct Reward {
    int32_t id;
    int32_t itemTypes[4];
    int32_t itemIds[4];
    int32_t money;
    int32_t fusionmatter;

    Reward(int32_t id, nlohmann::json types, nlohmann::json ids, int32_t m, int32_t fm) :
    id(id), money(m), fusionmatter(fm) {
        for (int i = 0; i < 4; i++) {
            itemTypes[i] = types[i];
            itemIds[i] = ids[i];
        }
    };
};

namespace MissionManager {
    extern std::map<int32_t, Reward*> Rewards;
    void init();

    void acceptMission(CNSocket* sock, CNPacketData* data);
    void completeMission(CNSocket* sock, CNPacketData* data);
    void setMission(CNSocket* sock, CNPacketData* data);
    void quitMission(CNSocket* sock, CNPacketData* data);
}
