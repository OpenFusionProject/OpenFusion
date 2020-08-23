#pragma once

namespace settings {
    extern bool VERBOSE;
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
    extern std::string GMPASS;

    void init();
}
