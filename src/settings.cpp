#include <iostream>
#include "settings.hpp"
#include "contrib/INIReader.hpp"

// defaults :)
bool settings::VERBOSE = false;

int settings::LOGINPORT = 8001;
bool settings::LOGINRANDCHARACTERS = false;

int settings::SHARDPORT = 8002;
std::string settings::SHARDSERVERIP = "127.0.0.1";
int settings::VIEWDISTANCE = 20000;

// default spawn point is city hall
int settings::SPAWN_X = 179213;
int settings::SPAWN_Y = 268451;
int settings::SPAWN_Z = -4210;
std::string settings::GMPASS = "pass";
std::string settings::NPCJSON = "NPCs.json";
std::string settings::MOTDSTRING = "Welcome to OpenFusion!";

void settings::init() {
    INIReader reader("config.ini");

    if (reader.ParseError() != 0) {
        if (reader.ParseError() == -1)
            std::cerr << "[WARN] Settings: missing config.ini file!" << std::endl;
        else 
            std::cerr << "[WARN] Settings: invalid config.ini syntax at line " << reader.ParseError() << std::endl;

        return;
    }

    VERBOSE = reader.GetBoolean("", "verbose", VERBOSE);
    LOGINPORT = reader.GetInteger("login", "port", LOGINPORT);
    LOGINRANDCHARACTERS = reader.GetBoolean("login", "randomcharacters", LOGINRANDCHARACTERS);
    SHARDPORT = reader.GetInteger("shard", "port", SHARDPORT);
    SHARDSERVERIP = reader.Get("shard", "ip", "127.0.0.1");
    VIEWDISTANCE = reader.GetInteger("shard", "view", VIEWDISTANCE);
    SPAWN_X = reader.GetInteger("shard", "spawnx", SPAWN_X);
    SPAWN_Y = reader.GetInteger("shard", "spawny", SPAWN_Y);
    SPAWN_Z = reader.GetInteger("shard", "spawnz", SPAWN_Z);
    GMPASS = reader.Get("login", "pass", GMPASS);
    NPCJSON = reader.Get("shard", "npcdata", NPCJSON);
    MOTDSTRING = reader.Get("shard", "motd", MOTDSTRING);

}
