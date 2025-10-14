#include "common_defs.h"
#include <iostream>

namespace synapse {
    void log(const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    }
}
