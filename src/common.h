#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <iostream>

inline uint64_t get_curr_ms() {
    struct timespec tp = {0, 0};

    int err = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (err) {
        std::cout << "error in get_curr_ms" << std::endl;
        return 0;
    }

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000;
}

#endif
