#pragma once

#include "servers/CNShardServer.hpp"
#include "Player.hpp"

#include "JSON.hpp"

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

struct TaskData {
    /*
     * TODO: We'll probably want to keep only the data the server actually needs,
     *       but for now RE/development is much easier if we have everything at
     *       our fingertips.
     */
    nlohmann::json task;

    TaskData(nlohmann::json t) : task(t) {}

    // convenience
    auto operator[](std::string s) { return task[s]; }
};

namespace Missions {
    extern std::map<int32_t, Reward*> Rewards;
    extern std::map<int32_t, TaskData*> Tasks;
    extern nlohmann::json AvatarGrowth[37];
    void init();
    int findQSlot(Player *plr, int id);

    bool startTask(Player* plr, int TaskID);

    // checks if player doesn't have n/n quest items
    void updateFusionMatter(CNSocket* sock, int fusion);

    void mobKilled(CNSocket *sock, int mobid, int rolledQItem);

    void quitTask(CNSocket* sock, int32_t taskNum, bool manual);

    void failInstancedMissions(CNSocket* sock);
}
