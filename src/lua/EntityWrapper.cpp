#include "lua/LuaWrapper.hpp"
#include "lua/LuaManager.hpp"
#include "lua/EntityWrapper.hpp"

#include "Entities.hpp"

#define LIBNAME "Entity"
#define SUPERTBL "__entSUPERCLASSES"

#define ENTYGONESTR "Entity doesn't exist anymore!"

EntityRef* grabBaseEntityRef(lua_State *state, int indx) {
    // first, make sure its a userdata
    luaL_checktype(state, indx, LUA_TUSERDATA);

    // grab the super class table
    lua_getfield(state, LUA_REGISTRYINDEX, SUPERTBL);

    // grab the userdata
    EntityRef *data = (EntityRef*)lua_touserdata(state, indx);

    // check if it doesn't have a metatable
    if (!lua_getmetatable(state, indx)) {
        luaL_typerror(state, indx, LIBNAME);
        return NULL;
    }

    // index the super class table
    lua_gettable(state, -2);

    // if the index was nil, it doesn't exist
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        luaL_typerror(state, indx, LIBNAME);
        return NULL;
    }

    // it's good :)
    lua_pop(state, 1);
    return data;
}

// check at index 
EntityRef* grabEntityRef(lua_State *state, int indx) {
    EntityRef *ref = grabBaseEntityRef(state, indx);

    if (ref == NULL || !ref->isValid())
        return NULL;

    return ref;
}

// =============================================== [[ GETTERS ]] ===============================================

static int ent_getX(lua_State *state) {
    EntityRef *ref = grabEntityRef(state, 1);

    if (ref == NULL)
        return 0;

    lua_pushnumber(state, ref->getEntity()->x);
    return 1;
}

static int ent_getY(lua_State *state) {
    EntityRef *ref = grabEntityRef(state, 1);

    if (ref == NULL)
        return 0;

    lua_pushnumber(state, ref->getEntity()->y);
    return 1;
}

static int ent_getZ(lua_State *state) {
    EntityRef *ref = grabEntityRef(state, 1);

    if (ref == NULL)
        return 0;

    lua_pushnumber(state, ref->getEntity()->z);
    return 1;
}

static int ent_getType(lua_State *state) {
    EntityRef *ref = grabEntityRef(state, 1);

    if (ref == NULL)
        return 0;

    // push the type string
    switch(ref->type) {
        case EntityType::PLAYER:
            lua_pushstring(state, "Player");
            break;
        case EntityType::MOB:
            lua_pushstring(state, "Mob");
            break;
        case EntityType::EGG:
            lua_pushstring(state, "Egg");
            break;
        case EntityType::BUS:
            lua_pushstring(state, "Bus");
            break;
        default: // INVALID, COMBAT_NPC, SIMPLE_NPC
            lua_pushstring(state, "Entity");
            break;
    }
    return 1;
}

static luaL_Reg ent_getters[] = {
    {"x", ent_getX},
    {"y", ent_getY},
    {"z", ent_getZ},
    {"type", ent_getType},
    {0, 0}
};

// =============================================== [[ METHODS ]] ===============================================

static int ent_exists(lua_State *state) {
    EntityRef *data = (EntityRef*)grabBaseEntityRef(state, 1);

    lua_pushboolean(state, !(data == NULL || !data->isValid()));
    return 1;
}

static luaL_Reg ent_methods[] = {
    {"exists", ent_exists},
    {0, 0}
};

void LuaManager::Entity::init(lua_State *state) {
    // register our library as a global (and leave it on the stack)
    luaL_register(state, LIBNAME, ent_methods);
    lua_pop(state, 1); // pop library table

    // will hold our super classes
    lua_pushstring(state, SUPERTBL);
    lua_newtable(state);
    lua_rawset(state, LUA_REGISTRYINDEX);
}

void LuaManager::Entity::registerSuper(lua_State *state, const char *tname) {
    // grab the super class table
    lua_getfield(state, LUA_REGISTRYINDEX, SUPERTBL);

    // grab the metatable
    lua_getfield(state, LUA_REGISTRYINDEX, tname);
    lua_pushboolean(state, 1);

    // finally, set the index
    lua_rawset(state, -3);
    lua_pop(state, 1); // pop the super class table
}

void LuaManager::Entity::addGetters(lua_State *state) {
    luaL_register(state, NULL, ent_getters);
}

void LuaManager::Entity::addMethods(lua_State *state) {
    luaL_register(state, NULL, ent_methods);
}

void LuaManager::Entity::push(lua_State *state, EntityRef ref, const char *tname) {
    // creates the udata and copies the reference to the udata
    EntityRef *ent = (EntityRef*)lua_newuserdata(state, sizeof(EntityRef));
    *ent = ref;

    // attaches our metatable from the registry to the udata
    luaL_getmetatable(state, tname);
    lua_setmetatable(state, -2);
}