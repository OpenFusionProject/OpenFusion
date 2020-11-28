#include "CNLoginServer.hpp"
#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "ChatManager.hpp"
#include "MobManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "TransportManager.hpp"
#include "BuddyManager.hpp"
#include "Database.hpp"
#include "TableData.hpp"
#include "ChunkManager.hpp"
#include "GroupManager.hpp"
#include "Monitor.hpp"
#include "RacingManager.hpp"

#include "settings.hpp"

#include "../version.h"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.thread.h"
#else
#include <thread>
#endif
#include <string>
#include <chrono>
#include <signal.h>

// HACK
#ifdef __has_feature
#if __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__ 1
#endif
#endif

CNShardServer *shardServer = nullptr;
std::thread *shardThread = nullptr;

void startShard(CNShardServer* server) {
    server->start();
}

// terminate gracefully on SIGINT (for gprof & DB saving)
void terminate(int arg) {
    std::cout << "OpenFusion: terminating." << std::endl;

    if (shardServer != nullptr && shardThread != nullptr)
        shardServer->kill();
    
    Database::close();
    exit(0);
}

#ifndef _WIN32
void initsignals() {
    struct sigaction act;

    memset((void*)&act, 0, sizeof(act));
    sigemptyset(&act.sa_mask);

    // tell the OS to not kill us if you use a broken pipe, just let us know thru recv() or send()
    act.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &act, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }

    act.sa_handler = terminate;
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(1);
    }
}
#endif

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        std::cerr << "OpenFusion: WSAStartup failed" << std::endl;
        exit(EXIT_FAILURE);
    }
#else
    initsignals();
#endif
    srand(getTime());
    settings::init();
    std::cout << "[INFO] OpenFusion v" GIT_VERSION << std::endl;
    std::cout << "[INFO] Protocol version: " << PROTOCOL_VERSION << std::endl;
    std::cout << "[INFO] Intializing Packet Managers..." << std::endl;
    TableData::init();
    PlayerManager::init();
    ChatManager::init();
    MobManager::init();
    ItemManager::init();
    MissionManager::init();
    NanoManager::init();
    NPCManager::init();
    TransportManager::init();
    BuddyManager::init();
    GroupManager::init();
    RacingManager::init();
    Database::open();

    switch (settings::EVENTMODE) {
    case 0: break; // no event
    case 1: std::cout << "[INFO] Event active. Hey, Hey It's Knishmas!" << std::endl; break;
    case 2: std::cout << "[INFO] Event active. Wishing you a spook-tacular Halloween!" << std::endl; break;
    case 3: std::cout << "[INFO] Event active. Have a very hoppy Easter!" << std::endl; break;
    default:
        std::cout << "[FATAL] Unknown event set in config file." << std::endl;
        exit(1);
        /* not reached */
    }

    std::cout << "[INFO] Starting Server Threads..." << std::endl;
    CNLoginServer loginServer(settings::LOGINPORT);
    shardServer = new CNShardServer(settings::SHARDPORT);

    shardThread = new std::thread(startShard, (CNShardServer*)shardServer);

    loginServer.start();

    shardServer->kill();
    shardThread->join();

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

// helper functions

std::string U16toU8(char16_t* src) {
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
        return convert.to_bytes(src);
    } catch(const std::exception& e) {
        return "";
    }
}

// returns number of char16_t that was written at des
size_t U8toU16(std::string src, char16_t* des, size_t max) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
    std::u16string tmp = convert.from_bytes(src);

    // copy utf16 string to buffer
    if (sizeof(char16_t) * tmp.length() > max) // make sure we don't write outside the buffer
        memcpy(des, tmp.c_str(), sizeof(char16_t) * max);
    else
        memcpy(des, tmp.c_str(), sizeof(char16_t) * tmp.length());
    des[tmp.length()] = '\0';

    return tmp.length();
}

time_t getTime() {
    using namespace std::chrono;

    milliseconds value = duration_cast<milliseconds>((time_point_cast<milliseconds>(high_resolution_clock::now())).time_since_epoch());

    return (time_t)value.count();
}

// returns system time in seconds
time_t getTimestamp() {
    using namespace std::chrono;

    seconds value = duration_cast<seconds>((time_point_cast<seconds>(system_clock::now())).time_since_epoch());

    return (time_t)value.count();
}

// convert integer timestamp (in s) to FF systime struct
sSYSTEMTIME timeStampToStruct(uint64_t time) {

    const time_t timeProper = time;
    tm ts = *localtime(&timeProper);

    sSYSTEMTIME systime;
    systime.wMilliseconds = 0;
    systime.wSecond = ts.tm_sec;
    systime.wMinute = ts.tm_min;
    systime.wHour = ts.tm_hour;
    systime.wDay = ts.tm_mday;
    systime.wDayOfWeek = ts.tm_wday + 1;
    systime.wMonth = ts.tm_mon + 1;
    systime.wYear = ts.tm_year + 1900;

    return systime;
}
