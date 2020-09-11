#pragma once

namespace settings {
    extern int VERBOSITY;
    extern int LOGINPORT;
    extern bool LOGINRANDCHARACTERS;
    extern bool APPROVEALLNAMES;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern int PLAYERDISTANCE;
    extern int NPCDISTANCE;
    extern int SPAWN_X;
    extern int SPAWN_Y;
    extern int SPAWN_Z;
    extern std::string MOTDSTRING;
    extern std::string NPCJSON;
    extern std::string XDTJSON;
    extern std::string MOBJSON;
    extern std::string GMPASS;
    extern bool GM;

    void init();
}
