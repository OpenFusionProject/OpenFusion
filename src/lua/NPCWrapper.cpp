#include "lua/LuaWrapper.hpp"
#include "lua/NPCWrapper.hpp"

#include "NPC.hpp"
#include "NPCManager.hpp"
#include "Transport.hpp"

#define LIBNAME "NPC"
#define NPCGONESTR "NPC was destoryed and no longer exists!"
#define GETTERTBL "__npcGETTERS"
#define SETTERTBL "__npcSETTERS"
#define METHODTBL "__npcMETHODS"

static EntityRef* grabEntityRef(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // now, check and make sure its our library's metatable attached to this userdata
    EntityRef *ref = (EntityRef*)luaL_checkudata(state, indx, LIBNAME);
    if (ref == NULL) {
        luaL_typerror(state, indx, LIBNAME);
        return NULL;
    }

    // check if the npc exists still & return NULL if it doesn't
    if (!ref->isValid()) {
        luaL_argerror(state, indx, NPCGONESTR);
        return NULL;
    }

    return ref;
}

static CombatNPC* grabNPC(lua_State *state, int indx) {
    EntityRef *ref = grabEntityRef(state, indx);

    // if grabEntityRef failed, return error result
    return (ref == NULL) ? NULL : (CombatNPC*)ref->getEntity();
}

static int32_t grabID(lua_State *state, int indx) {
    EntityRef *ref = grabEntityRef(state, indx);

    // if grabEntityRef failed, return error result    
    return (ref == NULL) ? -1 : ref->id;
}

// =============================================== [[ GETTERS ]] ===============================================

static int npc_getMaxHealth(lua_State *state) {
    CombatNPC *npc = grabNPC(state, 1);

    // sanity check
    if (npc == NULL)
        return 0;

    lua_pushinteger(state, npc->maxHealth);
    return 1;
}

static const luaL_Reg npc_getters[] = {
    {"maxHealth", npc_getMaxHealth},
    {0, 0}
};

// =============================================== [[ SETTERS ]] ===============================================

static int npc_setMaxHealth(lua_State *state) {
    CombatNPC *npc = grabNPC(state, 1);
    int newMH = luaL_checkint(state, 2);

    // sanity check
    if (npc != NULL)
        npc->maxHealth = newMH;

    return 0;
}

static const luaL_Reg npc_setters[] = {
    {"maxHealth", npc_setMaxHealth},
    {0, 0}
};

// =============================================== [[ METHODS ]] ===============================================

static int npc_moveto(lua_State *state) {
    CombatNPC *npc = grabNPC(state, 1);
    int X = luaL_checkint(state, 2);
    int Y = luaL_checkint(state, 3);
    int Z = luaL_checkint(state, 4);

    // sanity check
    if (npc == NULL)
        return 0;

    std::queue<WarpLocation> queue;
    WarpLocation from = { npc->x, npc->y, npc->z };
    WarpLocation to = { X, Y, Z };

    // add a route to the queue; to be processed in Transport::stepNPCPathing()
    Transport::lerp(&queue, from, to, npc->speed);
    Transport::NPCQueues[npc->appearanceData.iNPC_ID] = queue;

    return 0;
}

static const luaL_Reg npc_methods[] = {
    {"moveTo", npc_moveto},
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

static int npc_new(lua_State *state) {
    int X = luaL_checkint(state, 1);
    int Y = luaL_checkint(state, 2);
    int Z = luaL_checkint(state, 3);
    int type = luaL_checkint(state, 4);
    int id = NPCManager::nextId++;

    // create & initalize the NPC
    CombatNPC *NPC = new CombatNPC(X, Y, Z, 0, INSTANCE_OVERWORLD, type, id, 1000);
    NPCManager::NPCs[id] = NPC;
    NPCManager::updateNPCPosition(id, X, Y, Z, INSTANCE_OVERWORLD, 0);

    // push it to the lua stack & return
    LuaManager::NPC::push(state, NPC);
    return 1;
}

void LuaManager::NPC::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, npc_methods);

    // sets NPC.new
    lua_pushstring(state, "new");
    lua_pushcfunction(state, npc_new);
    lua_rawset(state, -3);

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
    luaL_register(state, NULL, npc_methods);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the getters table
    lua_pushstring(state, GETTERTBL);
    lua_newtable(state);
    Entity::addGetters(state); // register the base Entity getters
    luaL_register(state, NULL, npc_getters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    // create the setters table
    lua_pushstring(state, SETTERTBL);
    lua_newtable(state);
    luaL_register(state, NULL, npc_setters);
    lua_rawset(state, LUA_REGISTRYINDEX);

    LuaManager::Entity::registerSuper(state, LIBNAME);
}

void LuaManager::NPC::push(lua_State *state, CombatNPC *npc) {
    Entity::push(state, EntityRef(npc->appearanceData.iNPC_ID), LIBNAME);
}