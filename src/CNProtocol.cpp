#include "CNProtocol.hpp"
#include "CNStructs.hpp"

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
        int sent = send(sock, (buffer_t*)(data + sentBytes), size - sentBytes, 0); // no flags defined
        if (SOCKETERROR(sent)) {
            if (OF_ERRNO == OF_EWOULD && maxTries > 0) {
                maxTries--;
                continue; // try again
            }
            std::cout << "[FATAL] SOCKET ERROR: " << OF_ERRNO << std::endl;
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

    if (readSize <= 0) {
        // we aren't reading a packet yet, try to start looking for one
        int recved = recv(sock, (buffer_t*)readBuffer, sizeof(int32_t), 0);
        if (!SOCKETERROR(recved)) {
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
            kill();
            return;
        }
    }

    if (readSize > 0 && readBufferIndex < readSize) {
        // read until the end of the packet! (or at least try too)
        int recved = recv(sock, (buffer_t*)(readBuffer + readBufferIndex), readSize - readBufferIndex, 0);
        if (!SOCKETERROR(recved))
            readBufferIndex += recved;
        else if (OF_ERRNO != OF_EWOULD) {
            // serious socket issue, disconnect connection
            kill();
            return;
        }
    }

    if (activelyReading && readBufferIndex - readSize <= 0) {
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

// ========================================================[[ CNServer ]]========================================================

void CNServer::init() {
    // create socket file descriptor
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKETINVALID(sock)) {
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
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    addressSize = sizeof(address);

    // Bind to the port
    if (SOCKETERROR(bind(sock, (struct sockaddr *)&address, addressSize))) {
        std::cerr << "[FATAL] OpenFusion: bind failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    if (SOCKETERROR(listen(sock, SOMAXCONN))) {
        std::cerr << "[FATAL] OpenFusion: listen failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // set server listener to non-blocking
#ifdef _WIN32
    unsigned long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
#else
    if (fcntl(sock, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
        std::cerr << "[FATAL] OpenFusion: fcntl failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

CNServer::CNServer() {};
CNServer::CNServer(uint16_t p): port(p) {}

void CNServer::start() {
    std::cout << "Starting server at *:" << port << std::endl;
    // listen to new connections, add to connection list
    while (active) {
        std::lock_guard<std::mutex> lock(activeCrit);

        // listen for a new connection
        SOCKET newConnectionSocket = accept(sock, (struct sockaddr *)&(address), (socklen_t*)&(addressSize));
        if (!SOCKETINVALID(newConnectionSocket)) {
            // new connection! make sure to set non-blocking!
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(newConnectionSocket, FIONBIO, &mode) != 0) {
#else
            if (fcntl(newConnectionSocket, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                std::cerr << "[WARN] OpenFusion: fcntl failed on new connection" << std::endl;
                #ifdef _WIN32
                    shutdown(newConnectionSocket, SD_BOTH);
                    closesocket(newConnectionSocket);
                #else
                    shutdown(newConnectionSocket, SHUT_RDWR);
                    close(newConnectionSocket);
                #endif
                continue;
            }

            std::cout << "New connection! " << inet_ntoa(address.sin_addr) << std::endl;

            // add connection to list!
            CNSocket* tmp = new CNSocket(newConnectionSocket, pHandler);
            connections.push_back(tmp);
            newConnection(tmp);
        }

        // for each connection, check if it's alive, if not kill it!
        std::list<CNSocket*>::iterator i = connections.begin();
        while (i != connections.end()) {
            CNSocket* cSock = *i;

            if (cSock->isAlive()) {
                cSock->step();

                ++i; // go to the next element
            } else {
                killConnection(cSock);
                connections.erase(i++);
                delete cSock;
            }
        }

        onStep();

#ifdef _WIN32
        Sleep(0);
#else
        sleep(0); // so your cpu isn't at 100% all the time, we don't need all of that! im not hacky! you're hacky!
#endif
    }
}

void CNServer::kill() {
    std::lock_guard<std::mutex> lock(activeCrit); // the lock will be removed when the function ends
    active = false;

    // kill all connections
    std::list<CNSocket*>::iterator i = connections.begin();
    while (i != connections.end()) {
        CNSocket* cSock = *i;

        if (cSock->isAlive()) {
            cSock->kill();
        }

        ++i; // go to the next element
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

void CNServer::newConnection(CNSocket* cns) {} // stubbed
void CNServer::killConnection(CNSocket* cns) {} // stubbed
void CNServer::onStep() {} // stubbed
