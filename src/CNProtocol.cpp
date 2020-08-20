#include "CNProtocol.hpp"
#include "CNStructs.hpp"

// ========================================================[[ CNSocketEncryption ]]========================================================

// literally C/P from the client and converted to C++ (does some byte swapping /shrug)
int CNSocketEncryption::Encrypt_byte_change_A(int ERSize, uint8_t* data, int size) {
    int num = 0;
    int num2 = 0;
    int num3 = 0;

    while (num + ERSize <= size)
    {
        int num4 = num + num3;
        int num5 = num + (ERSize - 1 - num3);

        uint8_t b = data[num4];
        data[num4] = data[num5];
        data[num5] = b;
        num += ERSize;
        num3++;
        if (num3 > ERSize / 2)
        {
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

CNPacketData::CNPacketData(void* b, uint32_t t, int l, uint64_t k): buf(b), type(t), size(l), key(k)  {}

CNPacketData::~CNPacketData() {
    free(buf); // we own the buffer
}

// ========================================================[[ CNSocket ]]========================================================

CNSocket::CNSocket(SOCKET s, PacketHandler ph): sock(s), pHandler(ph) {
    EKey = (uint64_t)(*(uint64_t*)&CNSocketEncryption::defaultKey[0]);
}

bool CNSocket::sendData(uint8_t* data, int size) {
    int sentBytes = 0;

    while (sentBytes < size) {
        int sent = send(sock, (buffer_t*)(data + sentBytes), size - sentBytes, 0); // no flags defined
        if (SOCKETERROR(sent)) 
            return false; // error occured while sending bytes
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

void CNSocket::sendPacket(CNPacketData* pak) {
    int tmpSize = pak->size + sizeof(uint32_t);
    uint8_t* tmpBuf = (uint8_t*)xmalloc(tmpSize);

    // copy packet type to the front of the buffer & then the actual buffer
    memcpy(tmpBuf, (void*)&pak->type, sizeof(uint32_t));
    memcpy(tmpBuf+sizeof(uint32_t), pak->buf, pak->size);

    // encrypt the packet
    CNSocketEncryption::encryptData((uint8_t*)tmpBuf, (uint8_t*)(&pak->key), tmpSize);

    // send packet size
    sendData((uint8_t*)&tmpSize, sizeof(uint32_t));

    // send packet data!
    sendData(tmpBuf, tmpSize);

    delete pak;
    free(tmpBuf); // free tmp buffer
}

void CNSocket::step() {
    if (readSize <= 0) {
        // we aren't reading a packet yet, try to start looking for one
        int recved = recv(sock, (buffer_t*)readBuffer, sizeof(int32_t), 0);
        if (!SOCKETERROR(recved)) {
            // we got out packet size!!!!
            readSize = *((int32_t*)readBuffer);
            // sanity check
            if (readSize > MAX_PACKETSIZE) {
                kill();
                return;
            }

            // we'll just leave bufferIndex at 0 since we already have the packet size, it's safe to overwrite those bytes
            activelyReading = true;
        }
    }
    
    if (readSize > 0 && readBufferIndex < readSize) {
        // read until the end of the packet! (or at least try too)
        int recved = recv(sock, (buffer_t*)(readBuffer + readBufferIndex), readSize - readBufferIndex, 0);
        if (!SOCKETERROR(recved))
            readBufferIndex += recved;
    }

    if (activelyReading && readBufferIndex - readSize <= 0) {            
        // decrypt readBuffer and copy to CNPacketData
        CNSocketEncryption::decryptData(readBuffer, (uint8_t*)(&EKey), readSize);

        // this doesn't leak memory because we free it in CNPacketData's deconstructor LOL
        void* tmpBuf = xmalloc(readSize-sizeof(int32_t));
        memcpy(tmpBuf, readBuffer+sizeof(uint32_t), readSize-sizeof(int32_t));
        CNPacketData tmp(tmpBuf, *((uint32_t*)readBuffer), readSize-sizeof(int32_t), EKey);

        // CALL PACKET HANDLER!!
        pHandler(this, &tmp); // tmp's deconstructor will be called when readStep returns so that tmpBuffer we made will be cleaned up :)

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

CNServer::CNServer() {
    lastTimer = getTime();
};
CNServer::CNServer(uint16_t p): port(p) {
    lastTimer = getTime();
}

void CNServer::start() {
    std::cout << "Starting server at *:" << port << std::endl;
    // listen to new connections, add to connection list
    while (active) {
        std::lock_guard<std::mutex> lock(activeCrit);

        // listen for a new connection
        SOCKET newConnection = accept(sock, (struct sockaddr *)&(address), (socklen_t*)&(addressSize));
        if (!SOCKETINVALID(newConnection)) {
            // new connection! make sure to set non-blocking!
#ifdef _WIN32
            unsigned long mode = 1;
            if (ioctlsocket(newConnection, FIONBIO, &mode) != 0) {
#else
            if (fcntl(newConnection, F_SETFL, (fcntl(sock, F_GETFL, 0) | O_NONBLOCK)) != 0) {
#endif
                std::cerr << "[WARN] OpenFusion: fcntl failed on new connection" << std::endl; 
                #ifdef _WIN32
                    shutdown(newConnection, SD_BOTH);
                    closesocket(newConnection);
                #else
                    shutdown(newConnection, SHUT_RDWR);
                    close(newConnection);
                #endif
                continue;
            }

            //std::cout << "New connection! " << inet_ntoa(address.sin_addr) << std::endl;

            // add connection to list!
            CNSocket* tmp = new CNSocket(newConnection, pHandler); 
            connections.push_back(tmp);
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

        if (getTime() - lastTimer > 2000) { // every 2 seconds call the onTimer method
            onTimer();
            lastTimer = getTime();
        }

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

void CNServer::killConnection(CNSocket* cns) {} // stubbed lol
void CNServer::onTimer() {} // stubbed lol
