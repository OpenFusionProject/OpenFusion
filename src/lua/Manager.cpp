#include "lua/Manager.hpp"

#include <iostream>

namespace LuaManager {
  void init() {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dostring(L, "print(\"Hello from Lua!\")")) {
      std::cout << "Lua error: " << lua_tostring(L, -1) << std::endl;
      lua_pop(L, 1);
    }
  }
}