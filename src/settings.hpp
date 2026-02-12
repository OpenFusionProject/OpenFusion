#pragma once

#include <stdint.h>
#include <string>
#include <time.h>

namespace settings {
    extern int VERBOSITY;
    extern bool SANDBOX;
    extern std::string SANDBOXEXTRAPATH;
    extern int LOGINPORT;
    extern bool APPROVEWHEELNAMES;
    extern bool APPROVECUSTOMNAMES;
    extern bool AUTOCREATEACCOUNTS;
    extern std::string AUTHMETHODS;
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
    extern std::string MONITORLISTENIP;
    extern int MONITORINTERVAL;
    extern bool DISABLEFIRSTUSEFLAG;
    extern bool IZRACESCORECAPPED;
    extern bool DROPFIXESENABLED;
    extern std::string MONITORPASS;
    extern int MAXPERIP;
    extern int LOGINRATELIMIT;

    void init();
}
