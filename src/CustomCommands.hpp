#pragma once

#include "core/Core.hpp"

namespace CustomCommands {
    void init();

    bool runCmd(std::string full, CNSocket* sock);
};
