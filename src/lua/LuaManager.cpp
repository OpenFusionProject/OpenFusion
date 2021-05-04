#include "lua/LuaManager.hpp"
#include "lua/LuaWrapper.hpp"
#include "lua/EventWrapper.hpp"
#include "lua/WorldWrapper.hpp"
#include "lua/PlayerWrapper.hpp"
#include "lua/NPCWrapper.hpp"

#include "PlayerManager.hpp"
#include "servers/CNShardServer.hpp"
#include "settings.hpp"

#include <filesystem>
#include <vector>

time_t getTime();
class Script;

// our "main" state, holds our environment
lua_State *LuaManager::global;
std::map<lua_State*, Script*> activeScripts;

/*
    Basically each script is treated as a coroutine, when wait() is called it gets yielded and is pushed onto the scheduler queue to be resumed.
*/
class Script {
private:
    lua_State *thread;
    lRegistry threadRef; // we'll need to unref this when closing this state

public: 
    Script(std::string source) {
        // make the thread & register it in the registry
        thread = lua_newthread(LuaManager::global);
        threadRef = luaL_ref(LuaManager::global, LUA_REGISTRYINDEX);

        // add this script to the map
        activeScripts[thread] = this;

        // compile & run the script, if it error'd, print the error
        int _retCode;
        if (luaL_loadfile(thread, source.c_str()) || ((_retCode = lua_resume(thread, 0)) != 0 && (_retCode != LUA_YIELD))) {
            std::cout << "[LUA ERROR]: " << lua_tostring(thread, -1) << std::endl; 
        }
    }

    // unregister all of our events from the wrappers
    ~Script() {
        LuaManager::clearState(thread);

        // remove it from the global registry
        luaL_unref(LuaManager::global, LUA_REGISTRYINDEX, threadRef);
    }

    // c++ moment....
    lua_State* getState() {
        return thread;
    }
};

struct scheduledThread {
    lRegistry ref; // ref that should be unref'd before resuming
    time_t time;
};
std::map<lua_State*, scheduledThread> scheduleQueue;

// pauses the script for x seconds, not very accurate but should be
// called within ~60ms or less of when it was schedueled
// will also return the time the thread was paused
int OF_wait(lua_State *state) {
    double seconds = luaL_checknumber(state, 1);

    // register the thread in the global registry so we don't get GC'd
    lua_pushthread(state);
    lRegistry ref = luaL_ref(state, LUA_REGISTRYINDEX); // threads decentant of a global state all share the global registry

    // yield the state and push the state onto our scheduler queue
    scheduleQueue[state] = {ref, (int)(seconds*1000) + getTime()};
    return lua_yield(state, 0);
}

void luaScheduler(CNServer *serv, time_t currtime) {
    for (auto iter = scheduleQueue.begin(); iter != scheduleQueue.end();) {
        time_t event = (*iter).second.time;
        lRegistry ref = (*iter).second.ref;
        lua_State *thread = (*iter).first;
        // is it time to run the event?
        if (event <= currtime) {
            // remove from the scheduler queue
            scheduleQueue.erase(iter++);

            // unregister the thread
            luaL_unref(thread, LUA_REGISTRYINDEX, ref);
            
            // resume the state, (wait() returns the delta time since call)
            lua_pushnumber(thread, ((double)currtime - event)/10);
            yieldCall(thread, 1);
        } else // go to the next iteration
            ++iter;
    }
}

void LuaManager::printError(std::string err) {
    std::cerr << "[LUA ERR]: " << err << std::endl;
}

void LuaManager::init() {
    // allocate our state
    global = luaL_newstate();

    // open lua's base libraries (excluding the IO for now)
    luaopen_base(global);
    luaopen_table(global);
    luaopen_string(global);
    luaopen_math(global);
    luaopen_debug(global);

    // add wait()
    lua_register(global, "wait", OF_wait);

    // register our libraries
    Event::init(global);
    World::init(global);
    Entity::init(global);
    Player::init(global);
    NPC::init(global);

    activeScripts = std::map<lua_State*, Script*>();

    // we want to be called after every poll(), so our timer delta is set to 0
    REGISTER_SHARD_TIMER(luaScheduler, 0);

    // load our scripts
    loadScripts();
}

void LuaManager::runScript(std::string filename) {
    new Script(filename);
}

void LuaManager::stopScripts() {
    // clear the scheduler queue
    scheduleQueue.clear();

    // free all the scripts, they'll take care of everything for us :)
    for (auto as : activeScripts) {
        delete as.second;
    }

    // finally clear the map
    activeScripts.clear();

    // walk through each player and unregister each event
    for (auto pair: PlayerManager::players) {
        if (pair.second->onChat != nullptr) {
            delete pair.second->onChat;
            pair.second->onChat = nullptr;
        }
    }
}

void LuaManager::loadScripts() {
    if (!std::filesystem::exists(settings::SCRIPTSDIR)) {
        std::cout << "[WARN] scripts directory \"" << settings::SCRIPTSDIR << "\" doesn't exist!" << std::endl;
        return;
    }

    // for each file in the scripts director, load the script
    std::filesystem::path dir(settings::SCRIPTSDIR);
    for (auto &d : std::filesystem::directory_iterator(dir)) {
        if (d.path().extension().u8string() == ".lua")
            runScript(d.path().u8string());
    }
}

void LuaManager::clearState(lua_State *state) {
    // TODO
}

void LuaManager::playerAdded(CNSocket *sock) {
    World::playerAdded(sock);
}

void LuaManager::playerRemoved(CNSocket *sock) {
    World::playerRemoved(sock);
}