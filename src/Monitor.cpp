#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "CNStructs.hpp"
#include "Monitor.hpp"
#include "settings.hpp"

#include <cstdio>

static int listener;
static std::mutex sockLock; // guards socket list
static std::list<SOCKET> sockets;
static sockaddr_in address;

// runs during init
void Monitor::init() {
    if (!settings::MONITORENABLED)
        return;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKETERROR(listener)) {
        std::cout << "Failed to create monitor socket" << std::endl;
        exit(1);
    }

#ifdef _WIN32
    const char opt = 1;
#else
    int opt = 1;
#endif
    if (SOCKETERROR(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))) {
        std::cout << "Failed to set SO_REUSEADDR on monitor socket" << std::endl;
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(settings::MONITORPORT);

    if (SOCKETERROR(bind(listener, (struct sockaddr*)&address, sizeof(address)))) {
        std::cout << "Failed to bind to monitor port" << std::endl;
        exit(1);
    }

    if (SOCKETERROR(listen(listener, SOMAXCONN))) {
        std::cout << "Failed to listen on monitor port" << std::endl;
        exit(1);
    }

#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(listener, FIONBIO, &mode) != 0) {
#else
    if (fcntl(listener, F_SETFL, (fcntl(listener, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
        std::cerr << "[FATAL] OpenFusion: fcntl failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Monitor listening on *:" << settings::MONITORPORT << std::endl;

    REGISTER_SHARD_TIMER(tick, settings::MONITORINTERVAL);
}

static bool transmit(std::list<SOCKET>::iterator& it, char *buff, int len) {
    int n = 0;
    int sock = *it;

    while (n < len) {
        n += send(sock, buff+n, len-n, 0);
        if (SOCKETERROR(n)) {
#ifdef _WIN32
            shutdown(sock, SD_BOTH);
            closesocket(sock);
#else
            shutdown(sock, SHUT_RDWR);
            close(sock);
#endif

            std::cout << "[INFO] Disconnected a monitor" << std::endl;

            it = sockets.erase(it);
            return false;
        }
    }

    return true;
}

// runs in shard thread
void Monitor::tick(CNServer *serv, time_t delta) {
    std::lock_guard<std::mutex> lock(sockLock);
    char buff[256];

    auto it = sockets.begin();
    while (it != sockets.end()) {
        if (!transmit(it, (char*)"begin\n", 6))
            continue;

        for (auto& pair : PlayerManager::players) {
            if (pair.second->hidden)
                continue;

            int n = std::snprintf(buff, sizeof(buff), "player %d %d %s\n",
                    pair.second->x, pair.second->y,
                    PlayerManager::getPlayerName(pair.second, false).c_str());

            if (!transmit(it, buff, n))
                continue;
        }

        if (!transmit(it, (char*)"end\n", 4))
            continue;

        it++;
    }
}

// runs in monitor thread
void Monitor::start(void *unused) {
    socklen_t len = sizeof(address);

    if (!settings::MONITORENABLED)
        return;

    for (;;) {
        int sock = accept(listener, (struct sockaddr*)&address, &len);
        if (SOCKETERROR(sock))
            continue;

        setSockNonblocking(listener, sock);

        std::cout << "[INFO] New monitor connection from " << inet_ntoa(address.sin_addr) << std::endl;

        {
            std::lock_guard<std::mutex> lock(sockLock);

            sockets.push_back(sock);
        }
    }
}
