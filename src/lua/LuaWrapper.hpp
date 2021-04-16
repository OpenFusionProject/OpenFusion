#pragma once
/* 
    This is a very simple header that adds a small "event" layer, where scripts can register callbacks & "waits" on events. The actual
    Event metatables & library is in EventWrapper.[hc]pp
*/

#include <string>
#include <map>
#include <unordered_set>
#include <cassert>

#include "lua/LuaManager.hpp"
#include "lua/PlayerWrapper.hpp"

#define yieldCall(state, nargs) \
    int _retCode = lua_resume(state, nargs); \
    if (_retCode != 0 && _retCode != LUA_YIELD) \
       LuaManager::printError(lua_tostring(state, -1)); \

inline static int lua_autoPush(lua_State* state, int nargs) {
    // return the number of pushed arguments :)
    return nargs;
}

/*
    This function will automatically push all of the passed arguments onto the stack and returns the # of pushed values

    Supported datatypes are:
        double or int : LUA_TNUMBER
        char* or const char* : LUA_TSTRING
        bool : LUA_TBOOLEAN
        lRegistry : grabs the object from the lua registry and pushes it onto the stack
        CNSocket* : Pushes the Player Entity
*/
template<typename T, class... Rest>
inline static int lua_autoPush(lua_State* state, int nargs, T arg, Rest... rest) {
    // pick which branch to compile based on the type of arg
    if constexpr(std::is_same<T, int>::value || std::is_same<T, double>::value) {
        lua_pushnumber(state, (lua_Number)arg);
    } else if constexpr(std::is_same<T, char*>::value || std::is_same<T, const char*>::value) {
        lua_pushstring(state, (const char*)arg);
    } else if constexpr(std::is_same<T, lRegistry>::value) {
        // grab the value from the registry
        lua_rawgeti(state, LUA_REGISTRYINDEX, (int)arg);
    } else if constexpr(std::is_same<T, CNSocket*>::value) { // pushes a Player Entity
        LuaManager::Player::push(state, arg);
    } else if constexpr(std::is_same<T, bool>::value) {
        lua_pushboolean(state, arg);
    }

    // recursively call, expanding rest and pushing the left-most rvalue into arg
    return lua_autoPush(state, ++nargs, rest...);
}

enum eventType {
    EVENT_CALLBACK, // standard callback
    EVENT_WAIT // state needs to be resumed with the arguments
};

class lEvent;

extern std::unordered_set<lEvent*> activeEvents;

class lEvent {
private:
    struct rawEvent {
        lua_State *state;
        eventType type;
        lRegistry ref;
        bool disabled;
    };

    uint32_t nextId; // next ID
    std::map<uint32_t, rawEvent> events;

    uint32_t registerEvent(lua_State *state, lRegistry ref, eventType type) {
        uint32_t id = nextId++;

        assert(nextId < UINT32_MAX);
        events[id] = {state, type, ref, false};
        return id;
    }

    void freeEvent(rawEvent *event) {
        // the registry of all states is the same!
        luaL_unref(event->state, LUA_REGISTRYINDEX, event->ref);
    }

public:
    lEvent() {
        events = std::map<uint32_t, rawEvent>();
        nextId = 0; // start at 0
        activeEvents.insert(this);
    }

    ~lEvent() {
        // remove from the active set and disable all existing callbacks
        activeEvents.erase(this);
        clear();
    }

    uint32_t addCallback(lua_State *state, lRegistry ref) {
        return registerEvent(state, ref, EVENT_CALLBACK);
    }

    // yields the thread until the event is called
    uint32_t addWait(lua_State *state) {
        // push the thread onto the stack and register it in the global registry
        lua_pushthread(state);
        lRegistry ref = luaL_ref(state, LUA_REGISTRYINDEX);
        return registerEvent(state, ref, EVENT_WAIT);
    }

    // walks through the events and unregister them from the state
    void clear() {
        for (auto &pair : events) {
            freeEvent(&pair.second);
        }

        events.clear();
    }

    // free the event based on id
    void clear(uint32_t id) {
        auto iter = events.find(id);

        // sanity check
        if (iter == events.end())
            return;

        // free the event
        freeEvent(&(*iter).second);
        events.erase(iter);
    }

    // frees all events to this state
    void clear(lua_State *state) {
        for (auto iter = events.begin(); iter != events.end();) {
            rawEvent *event = &(*iter).second;

            // if this is our state, free this event and erase it from the map
            if (event->state == state) {
                freeEvent(event);
                events.erase(iter++);
            } else
                ++iter;
        }
    }

    void disconnectEvent(uint32_t id) {
        auto iter = events.find(id);

        // sanity check
        if (iter == events.end())
            return;
        
        (*iter).second.disabled = true;
    }

    void reconnectEvent(uint32_t id) {
        auto iter = events.find(id);

        // sanity check
        if (iter == events.end())
            return;
        
        (*iter).second.disabled = false;
    }

    bool isDisabled(uint32_t id) {
        auto iter = events.find(id);

        // sanity check
        if (iter == events.end())
            return true;
        
        return (*iter).second.disabled;
    }

    template<class... Args> inline void call(Args... args) {
        auto eventsClone = events; // so if a callback is added, we don't iterate into undefined behavior
        for (auto &pair : eventsClone) {
            rawEvent *event = &pair.second;

            // if the event is disabled, skip it
            if (event->disabled)
                continue;

            switch (event->type) {
                case EVENT_CALLBACK: {
                    // make thread for this callback
                    lua_State *nThread = lua_newthread(event->state);

                    // push the callable first, the push all the arguments
                    lua_rawgeti(nThread, LUA_REGISTRYINDEX, (int)event->ref);
                    int nargs = lua_autoPush(nThread, 0, args...);

                    // then call it :)
                    yieldCall(nThread, nargs);

                    // we can safely pop the thread off the stack now
                    lua_pop(event->state, 1);
                    break;
                }
                case EVENT_WAIT: {
                    // erase this event
                    freeEvent(&pair.second);
                    events.erase(pair.first);

                    // the :wait() will return the passed arguments
                    int nargs = lua_autoPush(event->state, 0, args...);
                    yieldCall(event->state, nargs);
                    break;
                }
                default:
                    std::cout << "[WARN] INVALID EVENT TYPE : " << event->type << std::endl;
                    break;
            }
        }
    }
};