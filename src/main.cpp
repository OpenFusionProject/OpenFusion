#include "CNLoginServer.hpp"
#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "ChatManager.hpp"
#include "ItemManager.hpp"
#include "MissionManager.hpp"
#include "NanoManager.hpp"
#include "NPCManager.hpp"
#include "Database.hpp"

#include "settings.hpp"

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.thread.h"
#else 
#include <thread>
#endif
#include <string>

CNShardServer *shardServer;
std::thread *shardThread;

void startShard(CNShardServer* server) {
    server->start();
}

// terminate gracefully on SIGINT (for gprof)
void terminate(int arg) {
    std::cout << "OpenFusion: terminating" << std::endl;
    shardServer->kill();
    shardThread->join();
    exit(0);
}

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        std::cerr << "OpenFusion: WSAStartup failed" << std::endl;
        exit(EXIT_FAILURE);
    }
#else
    // tell the OS to not kill us if you use a broken pipe, just let us know thru recv() or send()
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate); // TODO: use sigaction() instead
#endif
    settings::init();
    std::cout << "[INFO] Protocol version: " << PROTOCOL_VERSION << std::endl;
    std::cout << "[INFO] Intializing Packet Managers..." << std::endl;
    PlayerManager::init();
    ChatManager::init();
    ItemManager::init();
    MissionManager::init();
    NanoManager::init();
    NPCManager::init();

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
