cmake_minimum_required(VERSION 3.13)
project(OpenFusion)

set(CMAKE_CXX_STANDARD 17)

execute_process(COMMAND git describe --tags OUTPUT_VARIABLE GIT_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

# OpenFusion supports multiple packet/struct versions
# 104 is the default version to build which can be changed
# For example: cmake -B build -DPROTOCOL_VERSION=728
set(PROTOCOL_VERSION 104 CACHE STRING "The packet version to build")

add_compile_definitions(PROTOCOL_VERSION=${PROTOCOL_VERSION})

# Disallow in-source builds
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed. Please refer to the wiki for more information. Please remove the CMakeFiles folder and the CMakeCache.txt file.")
endif()

# Output binaries to the bin folder in the source directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Put CMake targets (ALL_BUILD/ZERO_CHECK) into a folder
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set the OpenFusion project as the default startup project for VS
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT openfusion)

if (WIN32)
	# Set the output binary name to winfusion to match the regular Makefile
	set(BIN_NAME winfusion)
else()
	set(BIN_NAME fusion)
endif()

include_directories(src vendor)

file(GLOB_RECURSE SOURCES src/**.[ch]pp vendor/**.[ch]pp vendor/**.[ch] version.h)

configure_file(version.h.in ${CMAKE_SOURCE_DIR}/version.h @ONLY)

add_executable(openfusion ${SOURCES})

set_target_properties(openfusion PROPERTIES OUTPUT_NAME ${BIN_NAME})

target_link_libraries(openfusion sqlite3)

# Makes it so config, tdata, etc. get picked up when starting via the debugger in VS
set_property(TARGET openfusion PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Use pthreads if not generating a VS solution or MinGW makefile (because MinGW will prefer Win32 threads)
# Checking if the compiler ID is MSVC will allow us to open the project as a CMake project in VS.
# It's not something you should do, but it's there if you need it...
if (NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT CMAKE_GENERATOR MATCHES "MinGW Makefiles")
	find_package(Threads REQUIRED)
	target_link_libraries(openfusion pthread)
endif()
