#pragma once

#include <string>

namespace settings {
    extern int VERBOSITY;
    extern bool SANDBOX;
    extern int LOGINPORT;
    extern bool APPROVEALLNAMES;
    extern bool AUTOCREATEACCOUNTS;
    extern bool USEAUTHCOOKIES;
    extern int DBSAVEINTERVAL;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern bool LOCALHOSTWORKAROUND;
    extern bool ANTICHEAT;
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
    extern std::string PATCHDIR;
    extern std::string ENABLEDPATCHES;
    extern std::string TDATADIR;
    extern int EVENTMODE;
    extern bool MONITORENABLED;
    extern int MONITORPORT;
    extern int MONITORINTERVAL;
    extern bool DISABLEFIRSTUSEFLAG;
    extern bool IZRACESCORECAPPED;

    void init();
}
