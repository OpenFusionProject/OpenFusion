CC=clang
CXX=clang++
# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
CFLAGS=-O3 #-g3 -fsanitize=address
CXXFLAGS=-Wall -std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
LDFLAGS=-lpthread -ldl
# specifies the name of our exectuable
SERVER=bin/fusion

# assign protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION?=104

# Windows-specific
WIN_CC=x86_64-w64-mingw32-gcc
WIN_CXX=x86_64-w64-mingw32-g++
WIN_CFLAGS=-O3 #-g3 -fsanitize=address
WIN_CXXFLAGS=-Wall -std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
WIN_LDFLAGS=-static -lws2_32 -lwsock32
WIN_SERVER=bin/winfusion.exe

CSRC=\
	src/contrib/bcrypt/bcrypt.c\
	src/contrib/bcrypt/crypt_blowfish.c\
	src/contrib/bcrypt/crypt_gensalt.c\
	src/contrib/bcrypt/wrapper.c\
	src/contrib/sqlite/sqlite3.c\

CXXSRC=\
	src/contrib/sqlite/sqlite3pp.cpp\
	src/contrib/sqlite/sqlite3ppext.cpp\
	src/ChatManager.cpp\
	src/CombatManager.cpp\
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
CHDR=\
	src/contrib/bcrypt/bcrypt.h\
	src/contrib/bcrypt/crypt_blowfish.h\
	src/contrib/bcrypt/crypt_gensalt.h\
	src/contrib/bcrypt/ow-crypt.h\
	src/contrib/bcrypt/winbcrypt.h\
	src/contrib/sqlite/sqlite3.h\
	src/contrib/sqlite/sqlite3ext.h\

CXXHDR=\
	src/contrib/bcrypt/BCrypt.hpp\
	src/contrib/sqlite/sqlite3pp.h\
	src/contrib/sqlite/sqlite3ppext.h\
	src/contrib/INIReader.hpp\
	src/contrib/JSON.hpp\
	src/ChatManager.hpp\
	src/CombatManager.hpp\
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

COBJ=$(CSRC:.c=.o)
CXXOBJ=$(CXXSRC:.cpp=.o)

OBJ=$(COBJ) $(CXXOBJ)

all: $(SERVER)

windows: $(SERVER)

# assign Windows-specific values if targeting Windows
windows : CC=$(WIN_CC)
windows : CXX=$(WIN_CXX)
windows : CFLAGS=$(WIN_CFLAGS)
windows : CXXFLAGS=$(WIN_CXXFLAGS)
windows : LDFLAGS=$(WIN_LDFLAGS)
windows : SERVER=$(WIN_SERVER)

.SUFFIX: .o .c .cpp .hpp

.c.o: $(CHDR)
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o: $(CXXHDR)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) $(CHDR) $(CXXHDR)
	mkdir -p bin
	$(CXX) $(OBJ) $(LDFLAGS) -o $(SERVER)

.PHONY: all windows clean

clean:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER)
