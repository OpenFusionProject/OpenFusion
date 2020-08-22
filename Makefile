CXX=clang++
# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
CXXFLAGS=-std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
LDFLAGS=-lpthread
# specifies the name of our exectuable
SERVER=bin/fusion

# assign protocol version
# this can be overriden by ex. make PROTOCOL_VERSION=728
PROTOCOL_VERSION?=104

# Windows-specific
WIN_CXX=x86_64-w64-mingw32-g++
WIN_CXXFLAGS=-std=c++17 -O3 -DPROTOCOL_VERSION=$(PROTOCOL_VERSION) #-g3 -fsanitize=address
WIN_LDFLAGS=-static -lws2_32 -lwsock32
WIN_SERVER=bin/winfusion.exe

# source files
SRC=\
	src/ChatManager.cpp\
	src/CNLoginServer.cpp\
	src/CNProtocol.cpp\
	src/CNShardServer.cpp\
	src/CNShared.cpp\
	src/CNStructs.cpp\
	src/main.cpp\
	src/NanoManager.cpp\
	src/ItemManager.cpp\
	src/NPCManager.cpp\
	src/Player.cpp\
	src/PlayerManager.cpp\
	src/settings.cpp\

# headers (for timestamp purposes)
HDR=\
	src/ChatManager.hpp\
	src/CNLoginServer.hpp\
	src/CNProtocol.hpp\
	src/CNShardServer.hpp\
	src/CNShared.hpp\
	src/CNStructs.hpp\
	src/INIReader.hpp\
	src/NanoManager.hpp\
	src/ItemManager.hpp\
	src/NPCManager.hpp\
	src/Player.hpp\
	src/PlayerManager.hpp\
	src/settings.hpp\

OBJ=$(SRC:.cpp=.o)

all: $(SERVER)

windows: $(SERVER)

# assign Windows-specific values if targeting Windows
windows : CXX=$(WIN_CXX)
windows : CXXFLAGS=$(WIN_CXXFLAGS)
windows : LDFLAGS=$(WIN_LDFLAGS)
windows : SERVER=$(WIN_SERVER)

%.o: %.cpp $(HDR)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) $(HDR)
	mkdir -p bin
	$(CXX) $(OBJ) $(LDFLAGS) -o $(SERVER)

.PHONY: all windows clean

clean:
	rm -f $(OBJ) $(SERVER) $(WIN_SERVER)
