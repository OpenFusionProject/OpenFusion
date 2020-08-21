/* 
    CNStructs.hpp - defines some basic structs & useful methods for packets used by FusionFall based on the version defined
*/

#ifndef _CNS_HPP
#define _CNS_HPP

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

// TODO: rewrite U16toU8 & U8toU16 to not use codecvt

std::string U16toU8(char16_t* src);
int U8toU16(std::string src, char16_t* des); // returns number of char16_t that was written at des
uint64_t getTime();

// The CNPROTO_OVERRIDE definition is defined by cmake if you use it.
// If you don't use cmake, feel free to comment this out and change it around.
// Otherwise, use the PACKET_VERSION option (e.g. -DPACKET_VERSION=0104 in the cmake command) to change it.
#if !defined(CNPROTO_OVERRIDE)
    //#define CNPROTO_VERSION_0728
    #define CNPROTO_VERSION_0104
#endif

#if defined(CNPROTO_VERSION_0104)
    #include "structs/0104.hpp"
#elif defined(CNPROTO_VERSION_0728)
    #include "structs/0728.hpp"
#endif

#endif
