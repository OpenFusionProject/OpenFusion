/* 
    CNStructs.hpp - defines some basic structs & useful methods for packets used by FusionFall

    NOTE: this is missing the vast majority of packets, I have also ommitted the ERR & FAIL packets for simplicity
*/

#ifndef _CNS_HPP
#define _CNS_HPP

#include <iostream>
#include <stdio.h>
#include <stdint.h>
// Can't use this in MSVC.
#ifndef _MSC_VER
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <cstring>
#include <string> 
#include <locale> 
#include <codecvt> 

// TODO: rewrite U16toU8 & U8toU16 to not use codecvt

std::string U16toU8(char16_t* src);

// returns number of char16_t that was written at des
int U8toU16(std::string src, char16_t* des);

uint64_t getTime();

//#define CNPROTO_VERSION_0728
#define CNPROTO_VERSION_0104

#ifdef CNPROTO_VERSION_0728
	#define AEQUIP_COUNT 12
#else
	#define AEQUIP_COUNT 9
#endif

#define AINVEN_COUNT 50

// ========================================================[[ beta-20100104 ]]========================================================
#ifdef CNPROTO_VERSION_0104
	#include "structs/0104.hpp"
#endif

#endif
