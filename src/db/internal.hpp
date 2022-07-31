#pragma once

#include "db/Database.hpp"
#include <sqlite3.h>

extern std::mutex dbCrit;
extern sqlite3 *db;

using namespace Database;
