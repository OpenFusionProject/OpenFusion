#include "lua/LuaManager.hpp"
#include "lua/LuaWrapper.hpp"
#include "lua/EventWrapper.hpp"
#include "lua/WorldWrapper.hpp"
#include "core/CNStructs.hpp"

#include "PlayerManager.hpp"

static lEvent *addedEvent;
static lEvent *removedEvent;

#define LIBNAME "World"
#define GETTERTBL "__wrldGETTERS"
#define METHODTBL "__wrldMETHODS"

// =============================================== [[ GETTERS ]] ===============================================

int wrld_getPlrAdded(lua_State *state) {
    LuaManager::Event::push(state, addedEvent);
    return 1;
}

int wrld_getPlrRemoved(lua_State *state) {
    LuaManager::Event::push(state, removedEvent);
    return 1;
}

int wrld_getVersion(lua_State *state) {
    lua_pushnumber(state, PROTOCOL_VERSION);
    return 1;
}

int wrld_getPlayers(lua_State *state) {
    // create a new lua table and push it onto the stack
    int entries = 0;
    lua_newtable(state);

    // walk through the current list of players and add them to the table
    for (auto pair : PlayerManager::players) {
        lua_pushinteger(state, ++entries);
        LuaManager::Player::push(state, pair.first);
        lua_rawset(state, -3);
    }

    // returns the player table :)
    return 1;
}

// =============================================== [[ METHODS ]] ===============================================

int wrld_index(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, GETTERTBL);
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's not nil, call it and run the getter method
    if (!lua_isnil(state, -1)) {
        // push userdata & call the function
        lua_pushvalue(state, 1);
        lua_call(state, 1, 1);

        // return # of results
        return 1;
    }

    // grab the function from the methods lookup table
    lua_pop(state, 1);
    lua_pushstring(state, METHODTBL);
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // return result
    return 1;
}

static const luaL_Reg getters[] {
    {"onPlayerAdded", wrld_getPlrAdded},
    {"onPlayerRemoved", wrld_getPlrRemoved},
    {"players", wrld_getPlayers},
    {"version", wrld_getVersion},
    {0, 0}
};

// TODO
static const luaL_Reg methods[] = {
    //{"getNearbyPlayers", wrld_getNPlrs},
    {0, 0}
};


void LuaManager::World::init(lua_State *state) {
    lua_newtable(state);
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, wrld_index);
    lua_rawset(state, -3); // sets meta.__index = wrld_index
    lua_setmetatable(state, -2); // sets world.__metatable = meta
    lua_setglobal(state, LIBNAME);

    // setup the __wrldGETTERS table in the registry
    lua_pushstring(state, GETTERTBL);
    lua_newtable(state);
    luaL_register(state, NULL, getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // setup the __wrldMETHODS table in the registry
    lua_pushstring(state, METHODTBL);
    lua_newtable(state);
    luaL_register(state, NULL, methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    addedEvent = new lEvent();
    removedEvent = new lEvent();
}

void LuaManager::World::clearState(lua_State *state) {
    addedEvent->clear(state);
    removedEvent->clear(state);
}

void LuaManager::World::playerAdded(CNSocket *sock) {
    addedEvent->call(sock);
}

void LuaManager::World::playerRemoved(CNSocket *sock) {
    removedEvent->call(sock);
}