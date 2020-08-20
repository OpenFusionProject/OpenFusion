cmake_minimum_required(VERSION 3.16)
project(OpenFusion)

set(CMAKE_CXX_STANDARD 17)

# Disallow in-source builds
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "In-source builds not allowed. Please refer to the wiki for more information. Please remove the CMakeFiles folder and the CMakeCache.txt file.")
endif()

# Output binaries to the bin folder in the source directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

include_directories(src)

file(GLOB_RECURSE SOURCES src/**.cpp src/**.hpp)

add_executable(openfusion ${SOURCES})
