GIT_VERSION!=git describe --tags

CC=clang
CXX=clang++
# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
# If compiling with ASAN, invoke like this: $ LSAN_OPTIONS=suppressions=suppr.txt bin/fusion
CFLAGS=-O3 #-g3 -fsanitize=address
CXXFLAGS=-Wall -Wno-unknown-pragmas -std=c++17 -O2 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DGIT_VERSION=\"$(GIT_VERSION)\" -I./src -I./vendor #-g3 -fsanitize=address
LDFLAGS=-lpthread -lsqlite3 #-g3 -fsanitize=address
# specifies the name of our exectuable
SERVER=bin/fusion

# assign protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION?=104

# Windows-specific
WIN_CC=x86_64-w64-mingw32-gcc
WIN_CXX=x86_64-w64-mingw32-g++
WIN_CFLAGS=-O3 #-g3 -fsanitize=address
WIN_CXXFLAGS=-D_WIN32_WINNT=0x0601 -Wall -Wno-unknown-pragmas -std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) -DGIT_VERSION=\"$(GIT_VERSION)\" -I./src -I./vendor #-g3 -fsanitize=address
WIN_LDFLAGS=-static -lws2_32 -lwsock32 -lsqlite3 #-g3 -fsanitize=address
WIN_SERVER=bin/winfusion.exe

# C code; currently exclusively from vendored libraries
CSRC=\
	vendor/bcrypt/bcrypt.c\
	vendor/bcrypt/crypt_blowfish.c\
	vendor/bcrypt/crypt_gensalt.c\
	vendor/bcrypt/wrapper.c\

CHDR=\
	vendor/bcrypt/bcrypt.h\
	vendor/bcrypt/crypt_blowfish.h\
	vendor/bcrypt/crypt_gensalt.h\
	vendor/bcrypt/ow-crypt.h\
	vendor/bcrypt/winbcrypt.h\

CXXSRC=\
	src/core/CNProtocol.cpp\
	src/core/CNShared.cpp\
	src/core/Packets.cpp\
	src/servers/CNLoginServer.cpp\
	src/servers/CNShardServer.cpp\
	src/servers/Monitor.cpp\
	src/db/init.cpp\
	src/db/login.cpp\
	src/db/shard.cpp\
	src/db/player.cpp\
	src/db/email.cpp\
	src/Chat.cpp\
	src/CustomCommands.cpp\
	src/Entities.cpp\
	src/Email.cpp\
	src/Eggs.cpp\
	src/main.cpp\
	src/Missions.cpp\
	src/MobAI.cpp\
	src/Combat.cpp\
	src/Nanos.cpp\
	src/Abilities.cpp\
	src/Items.cpp\
	src/NPCManager.cpp\
	src/PlayerManager.cpp\
	src/PlayerMovement.cpp\
	src/BuiltinCommands.cpp\
	src/settings.cpp\
	src/Transport.cpp\
	src/TableData.cpp\
	src/Chunking.cpp\
	src/Buddies.cpp\
	src/Groups.cpp\
	src/Racing.cpp\
	src/Vendors.cpp\
	src/Trading.cpp\
	src/Rand.cpp\

# headers (for timestamp purposes)
CXXHDR=\
	src/core/CNProtocol.hpp\
	src/core/CNShared.hpp\
	src/core/CNStructs.hpp\
	src/core/Packets.hpp\
	src/core/Defines.hpp\
	src/core/Core.hpp\
	src/servers/CNLoginServer.hpp\
	src/servers/CNShardServer.hpp\
	src/servers/Monitor.hpp\
	src/db/Database.hpp\
	src/db/internal.hpp\
	vendor/bcrypt/BCrypt.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	src/Chat.hpp\
	src/CustomCommands.hpp\
	src/Entities.hpp\
	src/Email.hpp\
	src/Eggs.hpp\
	src/Missions.hpp\
	src/MobAI.hpp\
	src/Combat.hpp\
	src/Nanos.hpp\
	src/Abilities.hpp\
	src/Items.hpp\
	src/NPCManager.hpp\
	src/Player.hpp\
	src/PlayerManager.hpp\
	src/PlayerMovement.hpp\
	src/BuiltinCommands.hpp\
	src/settings.hpp\
	src/Transport.hpp\
	src/TableData.hpp\
	src/Chunking.hpp\
	src/Buddies.hpp\
	src/Groups.hpp\
	src/Racing.hpp\
	src/Vendors.hpp\
	src/Trading.hpp\
	src/Rand.hpp\

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
	rm -f src/*.o src/*/*.o $(SERVER) $(WIN_SERVER) version.h

# gets rid of all compiled objects, including the libraries
nuke:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER) version.h
