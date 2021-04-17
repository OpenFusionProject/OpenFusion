#include "lua/EntityWrapper.hpp"
#include "lua/EventWrapper.hpp"
#include "lua/PlayerWrapper.hpp"

#include "core/CNProtocol.hpp"
#include "Player.hpp"
#include "PlayerManager.hpp"

#define LIBNAME "Player"
#define PLRGONESTR "Player doesn't exist anymore, they left!"
#define GETTERTBL "__plrGETTERS"
#define SETTERTBL "__plrSETTERS"
#define METHODTBL "__plrMETHODS"

static EntityRef* grabEntityRef(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // now, check and make sure its our library's metatable attached to this userdata
    EntityRef *ref = (EntityRef*)luaL_checkudata(state, indx, LIBNAME);
    if (ref == NULL) {
        luaL_typerror(state, indx, LIBNAME);
        return NULL;
    }

    // check if the player exists still & return NULL if it doesn't
    if (!ref->isValid()) {
        luaL_argerror(state, indx, PLRGONESTR);
        return NULL;
    }

    return ref;
}

static Player* grabPlayer(lua_State *state, int indx) {
    EntityRef *ref = grabEntityRef(state, indx);

    if (ref == NULL)
        return NULL;

    return (Player*)ref->getEntity();
}

static CNSocket* grabSock(lua_State *state, int indx) {
    EntityRef *ref = grabEntityRef(state, indx);

    if (ref == NULL)
        return NULL;
    
    return ref->sock;
}

// =============================================== [[ GETTERS ]] ===============================================

static int plr_getName(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    if (plr == NULL)
        return 0;
    
    lua_pushstring(state, PlayerManager::getPlayerName(plr).c_str());
    return 1;
}

static int plr_getChatted(lua_State *state) {
    Player *plr = grabPlayer(state, 1);

    if (plr == NULL)
        return 0;
    
    // the Player* entity doesn't actually have an lEvent setup until a lua script asks for it, so
    // if Player->onChat is nullptr, create the lEvent and then push it :D
    if (plr->onChat == nullptr)
        plr->onChat = new lEvent();
    
    LuaManager::Event::push(state, plr->onChat);
    return 1;
}

static const luaL_Reg plr_getters[] = {
    {"name", plr_getName},
    {"onChat", plr_getChatted},
    {0, 0}
};

// =============================================== [[ SETTERS ]] ===============================================

static const luaL_Reg plr_setters[] = {
    {0, 0}
};

// =============================================== [[ METHODS ]] ===============================================

static int plr_kick(lua_State *state) {
    EntityRef *ref = grabEntityRef(state, 1);
    Player *plr;
    CNSocket *sock;

    // sanity check
    if (ref == NULL)
        return 0;

    plr = (Player*)ref->getEntity();
    sock = ref->sock;

    // construct packet
    INITSTRUCT(sP_FE2CL_REP_PC_EXIT_SUCC, response);

    response.iID = plr->iID;
    response.iExitCode = 3; // "a GM has terminated your connection"

    // send to target player
    sock->sendPacket(response, P_FE2CL_REP_PC_EXIT_SUCC);

    // ensure that the connection has terminated
    sock->kill();

    return 0;
}

static const luaL_Reg plr_methods[] = {
    {"kick", plr_kick},
    {0, 0}
};

// in charge of calling the correct getter method
static int plr_index(lua_State *state) {
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

// in charge of calling the correct setter method
static int plr_newindex(lua_State *state) {
    // grab the function from the getters lookup table
    lua_pushstring(state, SETTERTBL);
    lua_rawget(state, LUA_REGISTRYINDEX);
    lua_pushvalue(state, 2);
    lua_rawget(state, -2);

    // if it's nil return
    if (lua_isnil(state, -1))
        return 0;

    // push userdata & call the function
    lua_pushvalue(state, 1);
    lua_call(state, 1, 0);

    // return # of results
    return 0;
}

void LuaManager::Player::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, plr_methods);

    // create the meta table and populate it with our functions
    luaL_newmetatable(state, LIBNAME);
    lua_pushstring(state, "__index");
    lua_pushcfunction(state, plr_index);
    lua_rawset(state, -3); // sets meta.__index = plr_index
    lua_pushstring(state, "__newindex");
    lua_pushcfunction(state, plr_newindex);
    lua_rawset(state, -3); // sets meta.__newindex = plr_newindex
    lua_pop(state, 2); // pop meta & library table

    // create the methods table
    lua_pushstring(state, METHODTBL);
    lua_newtable(state);
    Entity::addMethods(state); // register the base Entity methods
    luaL_register(state, NULL, plr_methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the getters table
    lua_pushstring(state, GETTERTBL);
    lua_newtable(state);
    Entity::addGetters(state); // register the base Entity getters
    luaL_register(state, NULL, plr_getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the setters table
    lua_pushstring(state, SETTERTBL);
    lua_newtable(state);
    luaL_register(state, NULL, plr_setters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    LuaManager::Entity::registerSuper(state, LIBNAME);
}

void LuaManager::Player::push(lua_State *state, CNSocket *sock) {
    Entity::push(state, EntityRef(sock), LIBNAME);
}