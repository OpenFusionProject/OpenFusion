#include "CNStructs.hpp"
#include <chrono>

std::string U16toU8(char16_t* src) {
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
        return convert.to_bytes(src);
    } catch(const std::exception& e) {
        return "";
    }
}

// returns number of char16_t that was written at des
size_t U8toU16(std::string src, char16_t* des) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> convert;
    std::u16string tmp = convert.from_bytes(src);

    // copy utf16 string to buffer
    memcpy(des, tmp.c_str(), sizeof(char16_t) * tmp.length());
    des[tmp.length()] = '\0';

    return tmp.length();
}

time_t getTime() {
    using namespace std::chrono;

    milliseconds value = duration_cast<milliseconds>((time_point_cast<milliseconds>(high_resolution_clock::now())).time_since_epoch());

    return (time_t)value.count();
}
