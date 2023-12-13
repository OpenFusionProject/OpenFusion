#pragma once

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

struct LuaThread {
  lua_State *L;
  int ref;

  LuaThread(lua_State *L, int ref) : L(L), ref(ref) {}
};