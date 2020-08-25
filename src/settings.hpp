#pragma once

namespace settings {
    extern int VERBOSITY;
    extern int LOGINPORT;
    extern bool LOGINRANDCHARACTERS;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern int VIEWDISTANCE;
    extern int SPAWN_X;
    extern int SPAWN_Y;
    extern int SPAWN_Z;
    extern std::string MOTDSTRING;
    extern std::string NPCJSON;
    extern std::string WARPJSON;
    extern std::string GMPASS;
    extern bool GM;

    void init();
}
