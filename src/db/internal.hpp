#pragma once

#include "db/Database.hpp"
#include <sqlite3.h>

#if defined(__MINGW32__) && !defined(_GLIBCXX_HAS_GTHREADS)
    #include "mingw/mingw.mutex.h"
#else
    #include <mutex>
#endif

extern std::mutex dbCrit;
extern sqlite3 *db;

using namespace Database;
