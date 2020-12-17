#include <iostream>
#include "settings.hpp"
#include "contrib/INIReader.hpp"

// defaults :)
int settings::VERBOSITY = 1;

int settings::LOGINPORT = 23000;
bool settings::APPROVEALLNAMES = true;
int settings::DBSAVEINTERVAL = 240;

int settings::SHARDPORT = 23001;
std::string settings::SHARDSERVERIP = "127.0.0.1";
time_t settings::TIMEOUT = 60000;
int settings::VIEWDISTANCE = 25600;
bool settings::SIMULATEMOBS = true;

// default spawn point is Sector V (future)
int settings::SPAWN_X = 632032;
int settings::SPAWN_Y = 187177;
int settings::SPAWN_Z = -5500;
int settings::SPAWN_ANGLE = 130;
std::string settings::NPCJSON = "tdata/NPCs.json";
std::string settings::XDTJSON = "tdata/xdt.json";
std::string settings::MOBJSON = "tdata/mobs.json";
std::string settings::PATHJSON = "tdata/paths.json";
std::string settings::DROPSJSON = "tdata/drops.json";
std::string settings::EGGSJSON = "tdata/eggs.json";
std::string settings::GRUNTWORKJSON = "tdata/gruntwork.json";
std::string settings::MOTDSTRING = "Welcome to OpenFusion!";
std::string settings::DBPATH = "database.db";
int settings::ACCLEVEL = 1;
bool settings::DISABLEFIRSTUSEFLAG = true;

// monitor settings
bool settings::MONITORENABLED = false;
int settings::MONITORPORT = 8003;
int settings::MONITORINTERVAL = 5000;

// event mode settings
int settings::EVENTMODE = 0;
int settings::EVENTCRATECHANCE = 10;

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
    DBSAVEINTERVAL = reader.GetInteger("login", "dbsaveinterval", DBSAVEINTERVAL);
    SHARDSERVERIP = reader.Get("shard", "ip", "127.0.0.1");
    TIMEOUT = reader.GetInteger("shard", "timeout", TIMEOUT);
    VIEWDISTANCE = reader.GetInteger("shard", "viewdistance", VIEWDISTANCE);
    SIMULATEMOBS = reader.GetBoolean("shard", "simulatemobs", SIMULATEMOBS);
    SPAWN_X = reader.GetInteger("shard", "spawnx", SPAWN_X);
    SPAWN_Y = reader.GetInteger("shard", "spawny", SPAWN_Y);
    SPAWN_Z = reader.GetInteger("shard", "spawnz", SPAWN_Z);
    SPAWN_ANGLE = reader.GetInteger("shard", "spawnangle", SPAWN_ANGLE);
    NPCJSON = reader.Get("shard", "npcdata", NPCJSON);
    XDTJSON = reader.Get("shard", "xdtdata", XDTJSON);
    MOBJSON = reader.Get("shard", "mobdata", MOBJSON);
    DROPSJSON = reader.Get("shard", "dropdata", DROPSJSON);
    EGGSJSON = reader.Get("shard", "eggdata", EGGSJSON);
    PATHJSON = reader.Get("shard", "pathdata", PATHJSON);
    GRUNTWORKJSON = reader.Get("shard", "gruntwork", GRUNTWORKJSON);
    MOTDSTRING = reader.Get("shard", "motd", MOTDSTRING);
    DBPATH = reader.Get("shard", "dbpath", DBPATH);
    ACCLEVEL = reader.GetInteger("shard", "accountlevel", ACCLEVEL);
    EVENTMODE = reader.GetInteger("shard", "eventmode", EVENTMODE);
    EVENTCRATECHANCE = reader.GetInteger("shard", "eventcratechance", EVENTCRATECHANCE);
    DISABLEFIRSTUSEFLAG = reader.GetBoolean("shard", "disablefirstuseflag", DISABLEFIRSTUSEFLAG);
    MONITORENABLED = reader.GetBoolean("monitor", "enabled", MONITORENABLED);
    MONITORPORT = reader.GetInteger("monitor", "port", MONITORPORT);
    MONITORINTERVAL = reader.GetInteger("monitor", "interval", MONITORINTERVAL);
}
