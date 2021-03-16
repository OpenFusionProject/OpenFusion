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
	src/db/init.cpp\
	src/db/login.cpp\
	src/db/shard.cpp\
	src/db/player.cpp\
	src/db/email.cpp\
	src/Chat.cpp\
	src/CustomCommands.cpp\
	src/CNLoginServer.cpp\
	src/CNProtocol.cpp\
	src/CNShardServer.cpp\
	src/CNShared.cpp\
	src/Defines.cpp\
	src/Email.cpp\
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
	src/Monitor.cpp\
	src/Racing.cpp\
	src/Vendor.cpp\
	src/Trading.cpp\

# headers (for timestamp purposes)
CXXHDR=\
	vendor/bcrypt/BCrypt.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	vendor/INIReader.hpp\
	vendor/JSON.hpp\
	src/db/Database.hpp\
	src/db/internal.hpp\
	src/Chat.hpp\
	src/CustomCommands.hpp\
	src/CNLoginServer.hpp\
	src/CNProtocol.hpp\
	src/CNShardServer.hpp\
	src/CNShared.hpp\
	src/CNStructs.hpp\
	src/Defines.hpp\
	src/Email.hpp\
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
	src/Monitor.hpp\
	src/Racing.hpp\
	src/Vendor.hpp\
	src/Trading.hpp\

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
