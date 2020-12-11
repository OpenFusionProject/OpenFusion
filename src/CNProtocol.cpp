#include "CNProtocol.hpp"
#include "CNStructs.hpp"

#include <assert.h>

// ========================================================[[ CNSocketEncryption ]]========================================================

// literally C/P from the client and converted to C++ (does some byte swapping /shrug)
int CNSocketEncryption::Encrypt_byte_change_A(int ERSize, uint8_t* data, int size) {
    int num = 0;
    int num2 = 0;
    int num3 = 0;

    while (num + ERSize <= size) {
        int num4 = num + num3;
        int num5 = num + (ERSize - 1 - num3);

        uint8_t b = data[num4];
        data[num4] = data[num5];
        data[num5] = b;
        num += ERSize;
        num3++;
        if (num3 > ERSize / 2) {
            num3 = 0;
        }
    }

    num2 = ERSize - (num + ERSize - size);
    return num + num2;
}

int CNSocketEncryption::xorData(uint8_t* buffer, uint8_t* key, int size) {
    // xor every 8 bytes with 8 byte key
    for (int i = 0; i < size; i++) {
        buffer[i] ^= key[i % keyLength];
    }

    return size;
}

uint64_t CNSocketEncryption::createNewKey(uint64_t uTime, int32_t iv1, int32_t iv2) {
    uint64_t num = (uint64_t)(iv1 + 1);
    uint64_t num2 = (uint64_t)(iv2 + 1);
    uint64_t dEKey = (uint64_t)(*(uint64_t*)&defaultKey[0]);
    return dEKey * (uTime * num * num2);
}

int CNSocketEncryption::encryptData(uint8_t* buffer, uint8_t* key, int size) {
    int eRSize = size % (keyLength / 2 + 1) * 2 + keyLength; // C/P from client
    int size2 = xorData(buffer, key, size);
    return Encrypt_byte_change_A(eRSize, buffer, size2);
}

int CNSocketEncryption::decryptData(uint8_t* buffer, uint8_t* key, int size) {
    int eRSize = size % (keyLength / 2 + 1) * 2 + keyLength; // size % of 18????
    int size2 = Encrypt_byte_change_A(eRSize, buffer, size);
    return xorData(buffer, key, size2);
}

// ========================================================[[ CNPacketData ]]========================================================

CNPacketData::CNPacketData(void* b, uint32_t t, int l): buf(b), size(l), type(t) {}

// ========================================================[[ CNSocket ]]========================================================

CNSocket::CNSocket(SOCKET s, PacketHandler ph): sock(s), pHandler(ph) {
    EKey = (uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]);
}

bool CNSocket::sendData(uint8_t* data, int size) {
    int sentBytes = 0;
    int maxTries = 10;

    while (sentBytes < size) {
        int sent = send(sock, (buffer_t*)(data + sentBytes), size - sentBytes, 0);
        if (SOCKETERROR(sent)) {
            if (OF_ERRNO == OF_EWOULD && maxTries > 0) {
                maxTries--;
                continue; // try again
            }
            printSocketError("send");
            return false; // error occured while sending bytes
        }
        sentBytes += sent;
    }

    return true; // it worked!
}

void CNSocket::setEKey(uint64_t k) {
    EKey = k;
}

void CNSocket::setFEKey(uint64_t k) {
    FEKey = k;
}

uint64_t CNSocket::getEKey() {
    return EKey;
}

uint64_t CNSocket::getFEKey() {
    return FEKey;
}

bool CNSocket::isAlive() {
    return alive;
}

void CNSocket::kill() {
    alive = false;
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif
}

// we don't own buf, TODO: queue packets up to send in step()
void CNSocket::sendPacket(void* buf, uint32_t type, size_t size) {
    if (!alive)
        return;

    size_t bodysize = size + sizeof(uint32_t);
    uint8_t* fullpkt = (uint8_t*)xmalloc(bodysize+4);
    uint8_t* body = fullpkt+4;
    memcpy(fullpkt, (void*)&bodysize, 4);

    // copy packet type to the front of the buffer & then the actual buffer
    memcpy(body, (void*)&type, sizeof(uint32_t));
    memcpy(body+sizeof(uint32_t), buf, size);

    // encrypt the packet
    switch (activeKey) {
        case SOCKETKEY_E:
            CNSocketEncryption::encryptData((uint8_t*)body, (uint8_t*)(&EKey), bodysize);
            break;
        case SOCKETKEY_FE:
            CNSocketEncryption::encryptData((uint8_t*)body, (uint8_t*)(&FEKey), bodysize);
            break;
        default: {
            free(fullpkt);
            DEBUGLOG(
                std::cout << "[WARN]: UNSET KEYTYPE FOR SOCKET!! ABORTING SEND" << std::endl;
            )
            return;
        }
    }

    // send packet data!
    if (alive && !sendData(fullpkt, bodysize+4))
        kill();

    free(fullpkt);
}

void CNSocket::setActiveKey(ACTIVEKEY key) {
    activeKey = key;
}

void CNSocket::step() {
    // read step

    // XXX NOTE: we must not recv() twice without a poll() inbetween
    if (readSize <= 0) {
        // we aren't reading a packet yet, try to start looking for one
        int recved = recv(sock, (buffer_t*)readBuffer, sizeof(int32_t), 0);
        if (recved == 0) {
            // the socket was closed normally
            kill();
        } else if (!SOCKETERROR(recved)) {
            // we got our packet size!!!!
            readSize = *((int32_t*)readBuffer);
            // sanity check
            if (readSize > CN_PACKET_BUFFER_SIZE) {
                kill();
                return;
            }

            // we'll just leave bufferIndex at 0 since we already have the packet size, it's safe to overwrite those bytes
            activelyReading = true;
        } else if (OF_ERRNO != OF_EWOULD) {
            // serious socket issue, disconnect connection
            printSocketError("recv");
            kill();
            return;
        }
    }

    if (readSize > 0 && readBufferIndex < readSize) {
        // read until the end of the packet! (or at least try too)
        int recved = recv(sock, (buffer_t*)(readBuffer + readBufferIndex), readSize - readBufferIndex, 0);
        if (recved == 0) {
            // the socket was closed normally
            kill();
        } else if (!SOCKETERROR(recved))
            readBufferIndex += recved;
        else if (OF_ERRNO != OF_EWOULD) {
            // serious socket issue, disconnect connection
            printSocketError("recv");
            kill();
            return;
        }
    }

    if (activelyReading && readBufferIndex >= readSize) {
        // decrypt readBuffer and copy to CNPacketData
        CNSocketEncryption::decryptData((uint8_t*)&readBuffer, (uint8_t*)(&EKey), readSize);

        void* tmpBuf = readBuffer+sizeof(uint32_t);
        CNPacketData tmp(tmpBuf, *((uint32_t*)readBuffer), readSize-sizeof(int32_t));

        // call packet handler!!
        pHandler(this, &tmp);

        // reset vars :)
        readSize = 0;
        readBufferIndex = 0;
        activelyReading = false;
    }
}

void printSocketError(const char *call) {
#ifdef _WIN32
    std::cerr << call << ": ";
   
    LPSTR lpMsgBuf = nullptr;  // string buffer
    DWORD errCode = WSAGetLastError(); // error code

    if (errCode == 0) {
        std::cerr << "no error code" << std::endl;
        return;
    }

    size_t bufSize = FormatMessageA( // actually get the error message
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errCode, // in
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, // out
        0, NULL);

    // convert buffer to string and output message to terminal
    std::string msg(lpMsgBuf, bufSize);
    std::cerr << msg; // newline included

    LocalFree(lpMsgBuf); // free the buffer
#else
    perror(call);
#endif
}

bool setSockNonblocking(SOCKET listener, SOCKET newSock) {
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(newSock, FIONBIO, &mode) != 0) {
#else
    if (fcntl(newSock, F_SETFL, (fcntl(newSock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
        printSocketError("fcntl");
        std::cerr << "[WARN] OpenFusion: fcntl failed on new connection" << std::endl;
#ifdef _WIN32
        shutdown(newSock, SD_BOTH);
        closesocket(newSock);
#else
        shutdown(newSock, SHUT_RDWR);
        close(newSock);
#endif
        return false;
    }

    return true;
}

// ========================================================[[ CNServer ]]========================================================

void CNServer::init() {
    // create socket file descriptor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKETINVALID(sock)) {
        printSocketError("socket");
        std::cerr << "[FATAL] OpenFusion: socket failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // attach socket to the port
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) != 0) {
#else
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
#endif
        std::cerr << "[FATAL] OpenFusion: setsockopt failed" << std::endl;
        printSocketError("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    addressSize = sizeof(address);

    // Bind to the port
    if (SOCKETERROR(bind(sock, (struct sockaddr *)&address, addressSize))) {
        std::cerr << "[FATAL] OpenFusion: bind failed" << std::endl;
        printSocketError("bind");
        exit(EXIT_FAILURE);
    }

    if (SOCKETERROR(listen(sock, SOMAXCONN))) {
        std::cerr << "[FATAL] OpenFusion: listen failed" << std::endl;
        printSocketError("listen");
        exit(EXIT_FAILURE);
    }

    // set server listener to non-blocking
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
    if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
        printSocketError("fcntl");
        std::cerr << "[FATAL] OpenFusion: fcntl failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // poll() configuration
    fds.reserve(STARTFDSCOUNT);
    fds.push_back({sock, POLLIN});
}

CNServer::CNServer() {};
CNServer::CNServer(uint16_t p): port(p) {}

void CNServer::addPollFD(SOCKET s) {
    fds.push_back({s, POLLIN});
}

void CNServer::removePollFD(int i) {
    auto it = fds.begin();
    while (it != fds.end() && it->fd != fds[i].fd)
        it++;
    assert(it != fds.end());

    fds.erase(it);
}

void CNServer::start() {
    std::cout << "Starting server at *:" << port << std::endl;
    while (active) {
        // the timeout is to ensure shard timers are ticking
        int n = poll(fds.data(), fds.size(), 50);
        if (SOCKETERROR(n)) {
#ifndef _WIN32
            if (errno == EINTR)
                continue;
#endif
            std::cout << "[FATAL] poll() returned error" << std::endl;
            printSocketError("poll");
            terminate(0);
        }

        for (int i = 0; i < fds.size() && n > 0; i++) {
            if (fds[i].revents == 0)
                continue; // nothing in this one; don't decrement n

            n--;

            // is it the listener?
            if (fds[i].fd == sock) {
                // any sort of error on the listener
                if (fds[i].revents & ~POLLIN) {
                    std::cout << "[FATAL] Error on listener socket" << std::endl;
                    terminate(0);
                }

                SOCKET newConnectionSocket = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addressSize);
                if (SOCKETINVALID(newConnectionSocket)) {
                    printSocketError("accept");
                    continue;
                }

                if (!setSockNonblocking(sock, newConnectionSocket))
                    continue;

                std::cout << "New connection! " << inet_ntoa(address.sin_addr) << std::endl;

                addPollFD(newConnectionSocket);

                // add connection to list!
                CNSocket* tmp = new CNSocket(newConnectionSocket, pHandler);
                connections[newConnectionSocket] = tmp;
                newConnection(tmp);

            } else if (checkExtraSockets(i)) {
                // no-op. handled in checkExtraSockets().

            } else {
                std::lock_guard<std::mutex> lock(activeCrit); // protect operations on connections

                // player sockets
                if (connections.find(fds[i].fd) == connections.end()) {
                    std::cout << "[WARN] Event on non-existant socket?" << std::endl;
                    continue; // just to be safe
                }

                CNSocket* cSock = connections[fds[i].fd];

                // kill the socket on hangup/error
                if (fds[i].revents & ~POLLIN)
                    cSock->kill();

                if (cSock->isAlive()) {
                    cSock->step();
                } else {
                    killConnection(cSock);
                    connections.erase(fds[i].fd);
                    delete cSock;

                    removePollFD(i);

                    // a new entry was moved to this position, so we check it again
                    i--;
                }
            }
        }

        onStep();
    }
}

void CNServer::kill() {
    std::lock_guard<std::mutex> lock(activeCrit); // the lock will be removed when the function ends
    active = false;

    // kill all connections
    for (auto& pair : connections) {
        CNSocket *cSock = pair.second;
        if (cSock->isAlive())
            cSock->kill();

        delete cSock;
    }

    connections.clear();
}

void CNServer::printPacket(CNPacketData *data, int type) {
    if (settings::VERBOSITY < 2)
        return;

    if (settings::VERBOSITY < 3) switch (data->type) {
    case P_CL2LS_REP_LIVE_CHECK:
    case P_CL2FE_REP_LIVE_CHECK:
    case P_CL2FE_REQ_PC_MOVE:
    case P_CL2FE_REQ_PC_JUMP:
    case P_CL2FE_REQ_PC_SLOPE:
    case P_CL2FE_REQ_PC_MOVEPLATFORM:
    case P_CL2FE_REQ_PC_MOVETRANSPORTATION:
    case P_CL2FE_REQ_PC_ZIPLINE:
    case P_CL2FE_REQ_PC_JUMPPAD:
    case P_CL2FE_REQ_PC_LAUNCHER:
    case P_CL2FE_REQ_PC_STOP:
        return;
    }

    std::cout << "OpenFusion: received " << Defines::p2str(type, data->type) << " (" << data->type << ")" << std::endl;
}

bool CNServer::checkExtraSockets(int i) { return false; } // stubbed
void CNServer::newConnection(CNSocket* cns) {} // stubbed
void CNServer::killConnection(CNSocket* cns) {} // stubbed
void CNServer::onStep() {} // stubbed
