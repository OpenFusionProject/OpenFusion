#include "CNShardServer.hpp"
#include "PlayerManager.hpp"
#include "CNStructs.hpp"
#include "Monitor.hpp"
#include "settings.hpp"

#include <cstdio>

#ifndef _WIN32

static int listener;
static std::mutex sockLock; // guards socket list
static std::list<int> sockets;
static sockaddr_in address;

// runs during init
void Monitor::init() {
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(settings::MONITORPORT);

    if (bind(listener, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listener, SOMAXCONN) < 0) {
        perror("listen");
        exit(1);
    }

    std::cout << "[INFO] Monitor listening on *:" << settings::MONITORPORT << std::endl;

    REGISTER_SHARD_TIMER(tick, settings::MONITORINTERVAL);
}

static bool transmit(std::list<int>::iterator& it, char *buff, int len) {
    int n = 0;
    int sock = *it;

    while (n < len) {
        n += write(sock, buff+n, len-n);
        if (n < 0) {
            perror("send");
            close(sock);

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

    for (;;) {
        int sock = accept(listener, (struct sockaddr*)&address, &len);
        if (sock < 0) {
            perror("accept");
            continue;
        }

        std::cout << "[INFO] New monitor connection from " << inet_ntoa(address.sin_addr) << std::endl;

        {
            std::lock_guard<std::mutex> lock(sockLock);

            sockets.push_back(sock);
        }
    }
}

#endif // _WIN32
