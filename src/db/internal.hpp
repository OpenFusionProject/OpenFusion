#pragma once

#include "db/Database.hpp"
#include <sqlite3.h>

#define MIN_SUPPORTED_SQLITE_NUMBER 3033000
#define MIN_SUPPORTED_SQLITE "3.33.0"
// we can't use this in #error, since it doesn't expand macros

// Compile-time libsqlite version check
#if SQLITE_VERSION_NUMBER < MIN_SUPPORTED_SQLITE_NUMBER
#error libsqlite version too old. Minimum compatible version: 3.33.0
#endif

extern std::mutex dbCrit;
extern sqlite3 *db;

using namespace Database;
