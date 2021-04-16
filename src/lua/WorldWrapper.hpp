#pragma once

#include "lua/LuaManager.hpp"

namespace LuaManager {
    namespace World {
        void init(lua_State *state);

        void clearState(lua_State *state);
        void playerAdded(CNSocket *sock);
        void playerRemoved(CNSocket *sock);
    }
}