# makefile for OpenFusion

OBJS = src/*.cpp # source files to compile
CC = clang++ # using GNU C++ compiler
WIN_CC = x86_64-w64-mingw32-g++ # using GNU C++ compiler

# -w suppresses all warnings (the part that's commented out helps me find memory leaks, it ruins performance though!)
COMPILER_FLAGS = -std=c++17 -o3 -g3 -fsanitize=address
WIN_COMPILER_FLAGS = -std=c++17 -o3 -static #-g3 -fsanitize=address

#LINKER_FLAGS specifies the libraries we're linking against (NONE, this is a single header library.)
LINKER_FLAGS = -lpthread
WIN_LINKER_FLAGS = -lws2_32 -lwsock32

#OBJ_NAME specifies the name of our exectuable
OBJ_NAME = bin/fusion # location of output for build
WIN_OBJ_NAME = bin/winfusion.exe # location of output for build

all:	$(OBJS) 
	$(CC) $(OBJS) $(COMPILER_FLAGS) $(LINKER_FLAGS) -o $(OBJ_NAME)

windows:	$(OBJS) 
	$(WIN_CC) $(OBJS) $(WIN_COMPILER_FLAGS) $(WIN_LINKER_FLAGS) -o $(WIN_OBJ_NAME)
