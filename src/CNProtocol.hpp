#ifndef _CNP_HPP
#define _CNP_HPP

#define MAX_PACKETSIZE 8192
#define DEBUGLOG(x) x 

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32 
// windows (UNTESTED)
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    typedef char buffer_t;
    //#define errno WSAGetLastError()
    #define SOCKETINVALID(x) (x == INVALID_SOCKET)
    #define SOCKETERROR(x) (x == SOCKET_ERROR)
#else
// posix platform
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    typedef void buffer_t;
    #define SOCKETINVALID(x) (x < 0)
    #define SOCKETERROR(x) (x == -1)
#endif
#include <fcntl.h>

#include <string>
#include <cstring>
#include <csignal>
#include <list>
#include <queue>

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else 
    #include <mutex>
#endif

/*
    Packets format (sent from the client):
        [4 bytes] - size of packet including the 4 byte packet type
        [size bytes] - Encrypted packet (byte swapped && xor'd with 8 byte key; see CNSocketEncryption)
            [4 bytes] - packet type (which is a combination of the first 4 bytes of the packet and a checksum in some versions)
            [structure]
*/

// error checking calloc wrapper
inline void* xmalloc(size_t sz) {
    void* res = calloc(1, sz);

    if (res == NULL) {
        std::cerr << "[FATAL] OpenFusion: calloc failed to allocate memory!" << std::endl;
        exit(EXIT_FAILURE);
    }

    return res;
}

namespace CNSocketEncryption {
    // you won't believe how complicated they made it in the client :facepalm:
    static constexpr const char* defaultKey = "m@rQn~W#";
    static const unsigned int keyLength = 8;

    int Encrypt_byte_change_A(int ERSize, uint8_t* data, int size);
    int xorData(uint8_t* buffer, uint8_t* key, int size);
    uint64_t createNewKey(uint64_t uTime, int32_t iv1, int32_t iv2);
    int encryptData(uint8_t* buffer, uint8_t* key, int size);
    int decryptData(uint8_t* buffer, uint8_t* key, int size);
}

class CNPacketData {
public:
    void* buf;
    int size;
    uint32_t type;
    uint64_t key;

    CNPacketData(void* b, uint32_t t, int l, uint64_t k);
    ~CNPacketData();
};

class CNSocket;
typedef void (*PacketHandler)(CNSocket* sock, CNPacketData* data);

class CNSocket {
private:
    uint64_t EKey;
    uint64_t FEKey;
    int32_t readSize = 0;
    uint8_t* readBuffer = new uint8_t[MAX_PACKETSIZE];
    int readBufferIndex = 0;
    bool activelyReading = false;
    bool alive = true;

    bool sendData(uint8_t* data, int size);

public:
    SOCKET sock;
    PacketHandler pHandler;

    CNSocket(SOCKET s, PacketHandler ph);

    void setEKey(uint64_t k);
    void setFEKey(uint64_t k);
    uint64_t getEKey();
    uint64_t getFEKey();

    void kill();
    void sendPacket(CNPacketData* pak);
    void step();
    bool isAlive();
};

// in charge of accepting new connections and making sure each connection is kept alive
class CNServer {
protected:
    std::list<CNSocket*> connections;
    std::mutex activeCrit;

    SOCKET sock;
    uint16_t port;
    socklen_t addressSize;
    struct sockaddr_in address;
    void init();

    bool active = true;
    long int lastTimer;

public:
    PacketHandler pHandler;

    CNServer();
    CNServer(uint16_t p);

    void start();
    void kill();
    virtual void killConnection(CNSocket* cns);
    virtual void onTimer(); // called every 2 seconds
};

#endif