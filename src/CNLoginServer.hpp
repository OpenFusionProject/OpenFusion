#pragma once

#include "CNProtocol.hpp"
#include "Defines.hpp"
#include "Player.hpp"

#include <map>

struct CNLoginData {
    std::map<int64_t, Player> characters;
    int64_t selectedChar;
    int userID; int slot;
};

enum class LOGINERRORID {
    database_error = 0,
    id_doesnt_exist = 1,
    id_and_password_do_not_match = 2,
    id_already_in_use = 3,
    login_error = 4,
    client_version_outdated = 6,
    you_are_not_an_authorized_beta_tester = 7,
    authentication_connection_error = 8,
    updated_euala_required = 9
};

// WARNING: THERE CAN ONLY BE ONE OF THESE SERVERS AT A TIME!!!!!! TODO: change loginSessions & packet handlers to be non-static
class CNLoginServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);
    static std::map<CNSocket*, CNLoginData> loginSessions;
      
    static bool isLoginDataGood(std::string login, std::string password);
    static bool isPasswordCorrect(std::string actualPassword, std::string tryPassword);
    static bool isAccountInUse(int accountId);
    //returns true if success
    static bool exitDuplicate(int accountId);
public:
    CNLoginServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);   
};
