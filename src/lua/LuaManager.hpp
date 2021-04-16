#pragma once

#include "core/CNProtocol.hpp"

#include <string>
#ifdef _WIN32
    #include <luajit/lua.hpp>
#else
    #include <luajit-2.1/lua.hpp>
#endif

typedef int lRegistry;

namespace LuaManager {
    extern lua_State *global;
    void init();
    void printError(std::string err);

    // runs the script in the passed file
    void runScript(std::string filename);
    void stopScripts();
    void loadScripts();

    // unregisters the events tied to this state with all wrappers
    void clearState(lua_State *state);
    void playerAdded(CNSocket *sock);
    void playerRemoved(CNSocket *sock);
}
