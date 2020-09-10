#pragma once

#include "CNShardServer.hpp"
#include "Player.hpp"

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

struct SUItem {
    int32_t itemIds[3];

    SUItem(nlohmann::json ids) {
        for (int i = 0; i < 3; i++) {
            itemIds[i] = ids[i];
        }
    }
};

struct QuestDropSet {
    int32_t mobIds[3];
    int32_t itemIds[3];

    QuestDropSet(nlohmann::json mobs, nlohmann::json items) {
        for (int i = 0; i < 3; i++) {
            mobIds[i] = mobs[i];
            itemIds[i] = items[i];
        }
    }
};

struct ItemCleanup {
    int32_t itemIds[4];

    ItemCleanup(nlohmann::json ids) {
        for (int i = 0; i < 4; i++) {
            itemIds[i] = ids[i];
        }
    }
};

namespace MissionManager {
    extern std::map<int32_t, Reward*> Rewards;
    extern std::map<int32_t, SUItem*> SUItems;
    extern std::map<int32_t, QuestDropSet*> QuestDropSets;
    extern std::map<int32_t, ItemCleanup*> ItemCleanups;
    void init();

    void acceptMission(CNSocket* sock, CNPacketData* data);
    void completeTask(CNSocket* sock, CNPacketData* data);
    void setMission(CNSocket* sock, CNPacketData* data);
    void quitMission(CNSocket* sock, CNPacketData* data);

    int findFreeQSlot(Player *plr);
    void dropQuestItem(CNSocket *sock, int task, int count, int id, int mobid);
    void giveMissionReward(CNSocket *sock, int task);
}
