#pragma once

#include "core/Core.hpp"

#include <string>

namespace CustomCommands {
    void init();

    bool runCmd(std::string full, CNSocket* sock);
};
