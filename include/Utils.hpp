
#pragma once

#include <chrono>

namespace utils {

    inline long long getTimestamp() {
         auto now = std::chrono::system_clock::now();

        auto duration = now.time_since_epoch();

        auto nanoSec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
        return nanoSec.count();
    }
}

