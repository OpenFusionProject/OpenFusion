#pragma once

#include "CNProtocol.hpp"

namespace CustomCommands {
    void init();

    bool runCmd(std::string full, CNSocket* sock);
};
