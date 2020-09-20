CC=clang
CXX=clang++
# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
CFLAGS=-O3 #-g3 -fsanitize=address
CXXFLAGS=-Wall -Wno-unknown-pragmas -std=c++17 -O2 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DGIT_VERSION=\"$(shell git describe --tags)\" #-g3 -fsanitize=address
LDFLAGS=-lpthread -ldl #-g3 -fsanitize=address
# specifies the name of our exectuable
SERVER=bin/fusion

# assign protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION?=104

# Windows-specific
WIN_CC=x86_64-w64-mingw32-gcc
WIN_CXX=x86_64-w64-mingw32-g++
WIN_CFLAGS=-O3 #-g3 -fsanitize=address
WIN_CXX_VANILLA_MINGW_OPT_DISABLES=-fno-tree-dce -fno-inline-small-functions
WIN_CXX_MSYS2_MINGW_OPT_DISABLES=-fno-tree-dce -fno-tree-fre -fno-tree-vrp -fno-ipa-sra
WIN_CXXFLAGS=-Wall -Wno-unknown-pragmas -std=c++17 -O3 $(WIN_CXX_OPT_DISABLES) -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DGIT_VERSION=\"$(shell git describe --tags)\" #-g3 -fsanitize=address
WIN_LDFLAGS=-static -lws2_32 -lwsock32 #-g3 -fsanitize=address
WIN_SERVER=bin/winfusion.exe

CSRC=\
	src/contrib/sqlite/sqlite3.c\
	src/contrib/bcrypt/bcrypt.c\
	src/contrib/bcrypt/crypt_blowfish.c\
	src/contrib/bcrypt/crypt_gensalt.c\
	src/contrib/bcrypt/wrapper.c\

CXXSRC=\
	src/ChatManager.cpp\
	src/CNLoginServer.cpp\
	src/CNProtocol.cpp\
	src/CNShardServer.cpp\
	src/CNShared.cpp\
	src/Database.cpp\
	src/Defines.cpp\
	src/main.cpp\
	src/MissionManager.cpp\
	src/MobManager.cpp\
	src/NanoManager.cpp\
	src/ItemManager.cpp\
	src/NPCManager.cpp\
	src/PlayerManager.cpp\
	src/settings.cpp\
	src/TransportManager.cpp\
	src/TableData.cpp\
	src/ChunkManager.cpp\

# headers (for timestamp purposes)
CHDR=\
	src/contrib/sqlite/sqlite3.h\
	src/contrib/sqlite/sqlite_orm.h\
	src/contrib/bcrypt/bcrypt.h\
	src/contrib/bcrypt/crypt_blowfish.h\
	src/contrib/bcrypt/crypt_gensalt.h\
	src/contrib/bcrypt/ow-crypt.h\
	src/contrib/bcrypt/winbcrypt.h\

CXXHDR=\
	src/contrib/bcrypt/BCrypt.hpp\
	src/contrib/INIReader.hpp\
	src/contrib/JSON.hpp\
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
	src/MobManager.hpp\
	src/NanoManager.hpp\
	src/ItemManager.hpp\
	src/NPCManager.hpp\
	src/Player.hpp\
	src/PlayerManager.hpp\
	src/settings.hpp\
	src/TransportManager.hpp\
	src/TableData.hpp\
	src/ChunkManager.hpp\

COBJ=$(CSRC:.c=.o)
CXXOBJ=$(CXXSRC:.cpp=.o)

OBJ=$(COBJ) $(CXXOBJ)

HDR=$(CHDR) $(CXXHDR)

all: $(SERVER)

windows: $(SERVER)

# assign Windows-specific values if targeting Windows
windows : CC=$(WIN_CC)
windows : CXX=$(WIN_CXX)
windows : CFLAGS=$(WIN_CFLAGS)
windows : CXXFLAGS=$(WIN_CXXFLAGS)
windows : LDFLAGS=$(WIN_LDFLAGS)
windows : SERVER=$(WIN_SERVER)
windows : WIN_CXX_OPT_DISABLES=$(if $(filter-out 10, $(shell $(WIN_CXX) -dumpversion | egrep -o ^[0-9]+)), $(WIN_CXX_VANILLA_MINGW_OPT_DISABLES), $(WIN_CXX_MSYS2_MINGW_OPT_DISABLES))

.SUFFIX: .o .c .cpp .h .hpp

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o:
	$(CXX) -c $(CXXFLAGS) -o $@ $<

# header timestamps are a prerequisite for OF object files
$(CXXOBJ): $(CXXHDR)

$(SERVER): $(OBJ) $(CHDR) $(CXXHDR)
	mkdir -p bin
	$(CXX) $(OBJ) $(LDFLAGS) -o $(SERVER)

# compatibility with how cmake injects GIT_VERSION
version.h:
	touch version.h

src/main.o: version.h

.PHONY: all windows clean nuke

# only gets rid of OpenFusion objects, so we don't need to
# recompile the libs every time
clean:
	rm -f src/*.o $(SERVER) $(WIN_SERVER) version.h

# gets rid of all compiled objects, including the libraries
nuke:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER) version.h
