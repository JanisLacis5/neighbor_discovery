#ifndef COMMON_H
#define COMMON_H

#include <net/ethernet.h>
#include <cstdint>
#include <cstdio>
#include <ctime>

/*
    Hard coded value so that there is no risk different machines having different naming
    (really unlikely scenario)
*/
// Every packet will be sent with a protocol that is meant for local development
#define ETH_PROTOCOL 0x88B5  // ETH_P_802_EX1 in net/ethernet.h file

inline int64_t get_curr_ms() {
    struct timespec tp = {0, 0};

    int err = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (err) {
        perror("get_curr_ms");
        return -1;
    }

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000;
}

inline uint64_t ntohll(uint64_t n) {
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) 
        return __builtin_bswap64(n);
    return n;
}

inline uint64_t htonll(uint64_t n) {
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        return __builtin_bswap64(n);
    return n;
}

#endif
