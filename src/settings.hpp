#pragma once

namespace settings {
    extern int VERBOSITY;
    extern int LOGINPORT;
    extern bool APPROVEALLNAMES;
    extern int DBSAVEINTERVAL;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern time_t TIMEOUT;
    extern int PLAYERDISTANCE;
    extern int NPCDISTANCE;
    extern int SPAWN_X;
    extern int SPAWN_Y;
    extern int SPAWN_Z;
    extern int SPAWN_ANGLE;
    extern std::string MOTDSTRING;
    extern std::string NPCJSON;
    extern std::string XDTJSON;
    extern std::string MOBJSON;
    extern bool GM;

    void init();
}
