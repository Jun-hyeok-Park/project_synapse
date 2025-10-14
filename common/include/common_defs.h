#pragma once
#include <string>
#include <iostream>

namespace synapse {
    inline void printBanner(const std::string& who) {
        std::cout << "=== " << who << " started ===" << std::endl;
    }
}
