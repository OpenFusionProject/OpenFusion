#include "lua/LuaWrapper.hpp"
#include "lua/LuaManager.hpp"
#include "lua/EntityWrapper.hpp"

namespace LuaManager {
    namespace Player {
        void init(lua_State *state);

        void push(lua_State *state, CNSocket *sock);
    }
}