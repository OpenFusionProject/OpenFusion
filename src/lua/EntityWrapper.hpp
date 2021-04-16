#pragma once

#include "lua/LuaWrapper.hpp"
#include "lua/LuaManager.hpp"

#include "Entities.hpp"

namespace LuaManager {
    namespace Entity {
        void init(lua_State *state);

        void registerSuper(lua_State *state, const char *tname);

        void addGetters(lua_State *state);
        void addMethods(lua_State *state);

        void push(lua_State *state, EntityRef ref, const char *tname);
    }
}