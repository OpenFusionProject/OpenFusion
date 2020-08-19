#ifndef _SETT_HPP
#define _SETT_HPP

namespace settings {
    extern int LOGINPORT;
    extern bool LOGINRANDCHARACTERS;
    extern int SHARDPORT;
    extern std::string SHARDSERVERIP;
    extern int VIEWDISTANCE;
    extern int SPAWN_X;
    extern int SPAWN_Y;
    extern int SPAWN_Z;
    extern std::string MOTDSTRING;

    void init();
}

#endif
