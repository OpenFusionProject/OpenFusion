CXX=clang++
CC=clang
# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
CXXFLAGS=-Wall -std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
LDFLAGS=-lpthread -ldl
# specifies the name of our exectuable
SERVER=bin/fusion

# assign protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION?=104

# Windows-specific
WIN_CXX=x86_64-w64-mingw32-g++
WIN_CXXFLAGS=-Wall -std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
WIN_LDFLAGS=-static -lws2_32 -lwsock32
WIN_SERVER=bin/winfusion.exe

# source files
ASM_SRC=\
	src/contrib/bcrypt/x86.S\

CC_SRC=\
	src/contrib/bcrypt/bcrypt.c\
	src/contrib/bcrypt/crypt_blowfish.c\
	src/contrib/bcrypt/crypt_gensalt.c\
	src/contrib/bcrypt/wrapper.c\
	src/contrib/sqlite/sqlite3.c\

CXX_SRC=\
	src/contrib/sqlite/sqlite3pp.cpp\
	src/contrib/sqlite/sqlite3ppext.cpp\
	src/contrib/json11.cpp\
	src/ChatManager.cpp\
	src/CNLoginServer.cpp\
	src/CNProtocol.cpp\
	src/CNShardServer.cpp\
	src/CNShared.cpp\
	src/CNStructs.cpp\
	src/Database.cpp\
	src/Defines.cpp\
	src/main.cpp\
	src/MissionManager.cpp\
	src/NanoManager.cpp\
	src/ItemManager.cpp\
	src/NPCManager.cpp\
	src/Player.cpp\
	src/PlayerManager.cpp\
	src/settings.cpp\

# headers (for timestamp purposes)
CC_HDR=\
	src/contrib/bcrypt/bcrypt.h\
	src/contrib/bcrypt/crypt_blowfish.h\
	src/contrib/bcrypt/crypt_gensalt.h\
	src/contrib/bcrypt/ow-crypt.h\
	src/contrib/bcrypt/winbcrypt.h\
	src/contrib/sqlite/sqlite3.h\
	src/contrib/sqlite/sqlite3ext.h\

CXX_HDR=\
	src/contrib/bcrypt/BCrypt.hpp\
	src/contrib/sqlite/sqlite3pp.h\
	src/contrib/sqlite/sqlite3ppext.h\
	src/contrib/INIReader.hpp\
	src/contrib/JSON.hpp\
	src/contrib/json11.hpp\
	src/ChatManager.hpp\
	src/CNLoginServer.hpp\
	src/CNProtocol.hpp\
	src/CNShardServer.hpp\
	src/CNShared.hpp\
	src/CNStructs.hpp\
	src/Database.hpp\
	src/Defines.hpp\
	src/contrib/INIReader.hpp\
	src/contrib/JSON.hpp\
	src/MissionManager.hpp\
	src/NanoManager.hpp\
	src/ItemManager.hpp\
	src/NPCManager.hpp\
	src/Player.hpp\
	src/PlayerManager.hpp\
	src/settings.hpp\

ASM_OBJ=$(ASM_SRC:.S=.o)
CC_OBJ=$(CC_SRC:.c=.o)
CXX_OBJ=$(CXX_SRC:.cpp=.o)

OBJ=$(ASM_OBJ) $(CC_OBJ) $(CXX_OBJ)

all: $(SERVER)

windows: $(SERVER)

# assign Windows-specific values if targeting Windows
windows : CXX=$(WIN_CXX)
windows : CC=$(WIN_CC)
windows : CXXFLAGS=$(WIN_CXXFLAGS)
windows : LDFLAGS=$(WIN_LDFLAGS)
windows : SERVER=$(WIN_SERVER)

$(ASM_OBJ): %.o: %.S
	$(CC) -c -o $@ $<

$(CC_OBJ): %.o: %.c $(CC_HDR)
	$(CC) -c -O3 -o $@ $<

$(CXX_OBJ): %.o: %.cpp $(CXX_HDR)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) $(CC_HDR) $(CXX_HDR)
	mkdir -p bin
	$(CXX) $(OBJ) $(LDFLAGS) -o $(SERVER)

.PHONY: all windows clean

clean:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER)