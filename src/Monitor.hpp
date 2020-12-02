#pragma once

#ifndef _WIN32

#include "CNProtocol.hpp"

#include <list>
#include <mutex>

namespace Monitor {
    void init();
    void tick(CNServer *, time_t);
    void start(void *);
};

#endif // _WIN32
