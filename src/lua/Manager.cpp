#include "lua/Manager.hpp"

#include <iostream>

using namespace LuaManager;

static lua_State *globalState = nullptr;

void LuaManager::init() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

static LuaThread *startScript(const char *script) {
    lua_State *L = lua_newthread(globalState);
    LuaThread *T = new LuaThread(L, luaL_ref(L, LUA_REGISTRYINDEX));

    if (luaL_dostring(L, script)) {
      std::cout << "Lua error: " << lua_tostring(L, -1) << std::endl;
      lua_pop(L, 1);
    }

    return T;
}

void LuaManager::init() {
    globalState = luaL_newstate();
    luaL_openlibs(globalState);

    delete startScript("print(\"Hello from Lua!\")");
}
