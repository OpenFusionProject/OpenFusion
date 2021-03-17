#pragma once

#include "core/Core.hpp"
#include "Player.hpp"

#include <map>

struct CNLoginData {
    int userID;
    time_t lastHeartbeat;
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

    static void login(CNSocket* sock, CNPacketData* data);
    static void nameCheck(CNSocket* sock, CNPacketData* data);
    static void nameSave(CNSocket* sock, CNPacketData* data);
    static void characterCreate(CNSocket* sock, CNPacketData* data);
    static void characterDelete(CNSocket* sock, CNPacketData* data);
    static void characterSelect(CNSocket* sock, CNPacketData* data);
    static void finishTutorial(CNSocket* sock, CNPacketData* data);
    static void changeName(CNSocket* sock, CNPacketData* data);
    static void duplicateExit(CNSocket* sock, CNPacketData* data);

    static bool isLoginDataGood(std::string login, std::string password);
    static bool isPasswordCorrect(std::string actualPassword, std::string tryPassword);
    static bool isAccountInUse(int accountId);
    static bool isCharacterNameGood(std::string Firstname, std::string Lastname);
    static void newAccount(CNSocket* sock, std::string userLogin, std::string userPassword, int32_t clientVerC);
    // returns true if success
    static bool exitDuplicate(int accountId);
public:
    CNLoginServer(uint16_t p);

    void newConnection(CNSocket* cns);
    void killConnection(CNSocket* cns);
    void onStep();
};
