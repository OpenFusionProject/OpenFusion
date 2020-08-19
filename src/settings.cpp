#include <iostream>
#include "settings.hpp"
#include "INIReader.hpp"

// defaults :)
int settings::LOGINPORT = 8001;
bool settings::LOGINRANDCHARACTERS = false;

int settings::SHARDPORT = 8002;
std::string settings::SHARDSERVERIP = "127.0.0.1";
int settings::VIEWDISTANCE = 20000;

// default spawn point is city hall
int settings::SPAWN_X = 179213;
int settings::SPAWN_Y = 268451;
int settings::SPAWN_Z = -4210;

std::string settings::MOTDSTRING;

void settings::init() {
    INIReader reader("config.ini");

    if (reader.ParseError() != 0) {
        if (reader.ParseError() == -1)
            std::cerr << "[WARN] Settings: missing config.ini file!" << std::endl;
        else 
            std::cerr << "[WARN] Settings: invalid config.ini syntax at line " << reader.ParseError() << std::endl;

        return;
    }

    LOGINPORT = reader.GetInteger("login", "port", LOGINPORT);
    LOGINRANDCHARACTERS = reader.GetBoolean("login", "randomcharacters", LOGINRANDCHARACTERS);
    SHARDPORT = reader.GetInteger("shard", "port", SHARDPORT);
    SHARDSERVERIP = reader.Get("shard", "ip", "127.0.0.1");
    VIEWDISTANCE = reader.GetInteger("shard", "view", VIEWDISTANCE);
    SPAWN_X = reader.GetInteger("shard", "spawnx", SPAWN_X);
    SPAWN_Y = reader.GetInteger("shard", "spawny", SPAWN_Y);
    SPAWN_Z = reader.GetInteger("shard", "spawnz", SPAWN_Z);
    MOTDSTRING = reader.Get("shard", "motd", "Welcome to OpenFusion!");

}
