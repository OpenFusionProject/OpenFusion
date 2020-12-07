#pragma once

namespace settings {
    extern int VERBOSITY;
    extern int LOGINPORT;
    extern bool APPROVEALLNAMES;
    extern int DBSAVEINTERVAL;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern time_t TIMEOUT;
    extern int VIEWDISTANCE;
    extern bool SIMULATEMOBS;
    extern int SPAWN_X;
    extern int SPAWN_Y;
    extern int SPAWN_Z;
    extern int SPAWN_ANGLE;
    extern int ACCLEVEL;
    extern std::string MOTDSTRING;
    extern std::string NPCJSON;
    extern std::string XDTJSON;
    extern std::string MOBJSON;
    extern std::string PATHJSON;
    extern std::string DROPSJSON;
    extern std::string EGGSJSON;
    extern std::string GRUNTWORKJSON;
    extern std::string DBPATH;
    extern int EVENTMODE;
    extern int EVENTCRATECHANCE;
    extern bool MONITORENABLED;
    extern int MONITORPORT;
    extern int MONITORINTERVAL;
    extern bool DISABLEFIRSTUSEFLAG;

    void init();
}
