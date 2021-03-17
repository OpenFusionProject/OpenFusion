/*
 * CNStructs.hpp - defines some basic structs & useful methods for packets used by FusionFall based on the version defined
 */

#pragma once

#ifdef _MSC_VER
    // codecvt_* is deprecated in C++17 and MSVC will throw an annoying warning because of that.
    // Defining this before anything else to silence it.
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#include <iostream>
#include <stdio.h>
#include <stdint.h>
#ifndef _MSC_VER
    #include <sys/time.h>
#else
// Can't use this in MSVC.
    #include <time.h>
#endif
#include <cstring>
#include <string>
#include <locale>
#include <codecvt>

// yes this is ugly, but this is needed to zero out the memory so we don't have random stackdata in our structs.
#define INITSTRUCT(T, x) T x; \
    memset(&x, 0, sizeof(T));

// macros to extract fields from instanceIDs
#define MAPNUM(x) ((x) & 0xffffffff)
#define PLAYERID(x) ((x) >> 32)

// typedef for chunk position tuple
typedef std::tuple<int, int, uint64_t> ChunkPos;

// TODO: rewrite U16toU8 & U8toU16 to not use codecvt

std::string U16toU8(char16_t* src);
size_t U8toU16(std::string src, char16_t* des, size_t max); // returns number of char16_t that was written at des
time_t getTime();
time_t getTimestamp();
void terminate(int);

// The PROTOCOL_VERSION definition is defined by the build system.
#if !defined(PROTOCOL_VERSION)
    #include "structs/0104.hpp"
#elif PROTOCOL_VERSION == 728
    #include "structs/0728.hpp"
#elif PROTOCOL_VERSION == 104
    #include "structs/0104.hpp"
#elif PROTOCOL_VERSION == 1013
    #include "structs/1013.hpp"
#else
    #error Invalid PROTOCOL_VERSION
#endif

sSYSTEMTIME timeStampToStruct(uint64_t time);
