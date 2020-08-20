cmake_minimum_required(VERSION 3.16)
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

include_directories(src)

file(GLOB_RECURSE SOURCES src/**.cpp src/**.hpp)

add_executable(openfusion ${SOURCES})

# Set the output binary name to winfusion to match the regular Makefile
set_target_properties(openfusion PROPERTIES OUTPUT_NAME winfusion)
