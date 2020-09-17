#include <iostream>
#include "settings.hpp"
#include "contrib/INIReader.hpp"

// defaults :)
int settings::VERBOSITY = 1;

int settings::LOGINPORT = 8001;
bool settings::APPROVEALLNAMES = true;
int settings::DBSAVEINTERVAL = 240;

int settings::SHARDPORT = 8002;
std::string settings::SHARDSERVERIP = "127.0.0.1";
int settings::PLAYERDISTANCE = 20000;
int settings::NPCDISTANCE = 16000;

// default spawn point is Sector V (future)
int settings::SPAWN_X = 632032;
int settings::SPAWN_Y = 187177;
int settings::SPAWN_Z = -5500;
int settings::SPAWN_ANGLE = 130;
std::string settings::NPCJSON = "tdata/NPCs.json";
std::string settings::XDTJSON = "tdata/xdt.json";
std::string settings::MOBJSON = "tdata/mobs.json";
std::string settings::MOTDSTRING = "Welcome to OpenFusion!";
bool settings::GM = false;

void settings::init() {
    INIReader reader("config.ini");

    if (reader.ParseError() != 0) {
        if (reader.ParseError() == -1)
            std::cerr << "[WARN] Settings: missing config.ini file!" << std::endl;
        else
            std::cerr << "[WARN] Settings: invalid config.ini syntax at line " << reader.ParseError() << std::endl;

        return;
    }

    APPROVEALLNAMES = reader.GetBoolean("", "acceptallcustomnames", APPROVEALLNAMES);
    VERBOSITY = reader.GetInteger("", "verbosity", VERBOSITY);
    LOGINPORT = reader.GetInteger("login", "port", LOGINPORT);
    SHARDPORT = reader.GetInteger("shard", "port", SHARDPORT);
    SHARDSERVERIP = reader.Get("shard", "ip", "127.0.0.1");
    DBSAVEINTERVAL = reader.GetInteger("login", "dbsaveinterval", DBSAVEINTERVAL);
    PLAYERDISTANCE = reader.GetInteger("shard", "playerdistance", PLAYERDISTANCE);
    NPCDISTANCE = reader.GetInteger("shard", "npcdistance", NPCDISTANCE);
    SPAWN_X = reader.GetInteger("shard", "spawnx", SPAWN_X);
    SPAWN_Y = reader.GetInteger("shard", "spawny", SPAWN_Y);
    SPAWN_Z = reader.GetInteger("shard", "spawnz", SPAWN_Z);
    SPAWN_ANGLE = reader.GetInteger("shard", "spawnangle", SPAWN_ANGLE);
    NPCJSON = reader.Get("shard", "npcdata", NPCJSON);
    XDTJSON = reader.Get("shard", "xdtdata", XDTJSON);
    MOBJSON = reader.Get("shard", "mobdata", MOBJSON);
    MOTDSTRING = reader.Get("shard", "motd", MOTDSTRING);
    GM = reader.GetBoolean("shard", "gm", GM);
}
