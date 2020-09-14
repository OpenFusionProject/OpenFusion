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

enum class LoginError {
    DATABASE_ERROR = 0,
    ID_DOESNT_EXIST = 1,
    ID_AND_PASSWORD_DO_NOT_MATCH = 2,
    ID_ALREADY_IN_USE = 3,
    LOGIN_ERROR = 4,
    CLIENT_VERSION_OUTDATED = 6,
    YOU_ARE_NOT_AN_AUTHORIZED_BETA_TESTER = 7,
    AUTHENTICATION_CONNECTION_ERROR = 8,
    UPDATED_EUALA_REQUIRED = 9
};

// WARNING: THERE CAN ONLY BE ONE OF THESE SERVERS AT A TIME!!!!!! TODO: change loginSessions & packet handlers to be non-static
class CNLoginServer : public CNServer {
private:
    static void handlePacket(CNSocket* sock, CNPacketData* data);
    static std::map<CNSocket*, CNLoginData> loginSessions;

    static bool isLoginDataGood(std::string login, std::string password);
    static bool isPasswordCorrect(std::string actualPassword, std::string tryPassword);
    static bool isAccountInUse(int accountId);
    static bool isCharacterNameGood(std::string Firstname, std::string Lastname);
    // returns true if success
    static bool exitDuplicate(int accountId);
public:
    CNLoginServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);
};
