#pragma once

#include "CNProtocol.hpp"

#include <list>
#include <mutex>

namespace Monitor {
    void init();
    void tick(CNServer *, time_t);
    void start(void *);
};
