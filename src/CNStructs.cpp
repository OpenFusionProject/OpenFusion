#include "CNStructs.hpp"

std::string U16toU8(char16_t* src) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert; 
    return convert.to_bytes(src);   
}

// returns number of char16_t that was written at des
int U8toU16(std::string src, char16_t* des) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert; 
    std::u16string tmp = convert.from_bytes(src);

    // copy utf16 string to buffer
    memcpy(des, tmp.c_str(), sizeof(char16_t) * tmp.length());
    des[tmp.length()] = '\0';

    return tmp.length();
}

uint64_t getTime() {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000 + tp.tv_usec / 1000;
}