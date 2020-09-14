#include "CNLoginServer.hpp"
#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "ChatManager.hpp"
#include "CombatManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "TransportManager.hpp"
#include "Database.hpp"
#include "TableData.hpp"

#include "settings.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.thread.h"
#else
#include <thread>
#endif
#include <string>
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

#ifndef _WIN32
// terminate gracefully on SIGINT (for gprof)
void terminate(int arg) {
    std::cout << "OpenFusion: terminating." << std::endl;

    if (shardServer != nullptr && shardThread != nullptr) {
        shardServer->kill();
        shardThread->join();
    }

#if defined(__SANITIZE_ADDRESS__)
    TableData::cleanup();
#endif

    exit(0);
}

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
    settings::init();
    std::cout << "[INFO] Protocol version: " << PROTOCOL_VERSION << std::endl;
    std::cout << "[INFO] Intializing Packet Managers..." << std::endl;
    TableData::init();
    PlayerManager::init();
    ChatManager::init();
    CombatManager::init();
    ItemManager::init();
    MissionManager::init();
    NanoManager::init();
    NPCManager::init();
    TransportManager::init();

    Database::open();

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
