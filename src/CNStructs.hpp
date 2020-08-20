/* 
    CNStructs.hpp - defines some basic structs & useful methods for packets used by FusionFall based on the version defined
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
int U8toU16(std::string src, char16_t* des); // returns number of char16_t that was written at des
uint64_t getTime();

//#define CNPROTO_VERSION_0728
#define CNPROTO_VERSION_0104

#if defined(CNPROTO_VERSION_0104)
	#include "structs/0104.hpp"
#elif defined(CNPROTO_VERSION_0728)
	#include "structs/0728.hpp"
#endif

#endif
