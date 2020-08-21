#include "NPCManager.hpp"
#include "settings.hpp"

#include <cmath>
#include <algorithm>
#include <list>

std::map<int32_t, BaseNPC> NPCManager::NPCs;

void NPCManager::init() {
    /* BaseNPC test(settings::SPAWN_X, settings::SPAWN_Y, settings::SPAWN_Z, 727);
    NPCs[test.appearanceData.iNPC_ID] = test; */
}

void NPCManager::updatePlayerNPCS(CNSocket* sock, PlayerView& view) {
    std::list<int32_t> yesView;
    std::list<int32_t> noView;

    for (auto pair : NPCs) {
        int diffX = abs(view.plr.x - pair.second.appearanceData.iX);
        int diffY = abs(view.plr.y - pair.second.appearanceData.iY);

        if (diffX < settings::VIEWDISTANCE && diffY < settings::VIEWDISTANCE) {
            yesView.push_back(pair.first);
        } else {
            noView.push_back(pair.first);
        }
    }

    std::list<int32_t>::iterator i = view.viewableNPCs.begin();
    while (i != view.viewableNPCs.end()) {
        int32_t id = *i;

        if (std::find(noView.begin(), noView.end(), id) != noView.end()) {
            // it shouldn't be visible, send NPC_EXIT
            sP_FE2CL_NPC_EXIT* exitData = (sP_FE2CL_NPC_EXIT*)xmalloc(sizeof(sP_FE2CL_NPC_EXIT));
            
            exitData->iNPC_ID = id;

            sock->sendPacket(new CNPacketData((void*)exitData, P_FE2CL_NPC_EXIT, sizeof(sP_FE2CL_NPC_EXIT), sock->getFEKey()));
            
            // remove from view
            view.viewableNPCs.erase(i++);
        }

        ++i;
    }

    for (int32_t id : yesView) {
        if (std::find(view.viewableNPCs.begin(), view.viewableNPCs.end(), id) == view.viewableNPCs.end()) {

            // needs to be added to viewableNPCs! send NPC_ENTER
            sP_FE2CL_NPC_ENTER* enterData = (sP_FE2CL_NPC_ENTER*)xmalloc(sizeof(sP_FE2CL_NPC_ENTER));

            enterData->NPCAppearanceData = NPCs[id].appearanceData;

            sock->sendPacket(new CNPacketData((void*)enterData, P_FE2CL_NPC_ENTER, sizeof(sP_FE2CL_NPC_ENTER), sock->getFEKey()));
            
            view.viewableNPCs.push_back(id);
        }
    }

    PlayerManager::players[sock].viewableNPCs = view.viewableNPCs;
}