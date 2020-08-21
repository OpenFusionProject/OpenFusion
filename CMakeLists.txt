cmake_minimum_required(VERSION 3.13)
project(OpenFusion)

set(CMAKE_CXX_STANDARD 17)

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

include_directories(src)

file(GLOB_RECURSE SOURCES src/**.cpp src/**.hpp)

add_executable(openfusion ${SOURCES})

set_target_properties(openfusion PROPERTIES OUTPUT_NAME ${BIN_NAME})

# Use pthreads if not generating a VS solution or MinGW makefile (because MinGW will prefer Win32 threads)
# Checking if the compiler ID is MSVC will allow us to open the project as a CMake project in VS.
# It's not something you should do, but it's there if you need it...
if (NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT CMAKE_GENERATOR MATCHES "MinGW Makefiles")
	find_package(Threads REQUIRED)
	target_link_libraries(openfusion pthread)
endif()