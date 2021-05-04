#include "lua/LuaWrapper.hpp"
#include "lua/LuaManager.hpp"
#include "lua/EntityWrapper.hpp"

namespace LuaManager {
    namespace NPC {
        void init(lua_State *state);

        void push(lua_State *state, CombatNPC *npc);
    }
}