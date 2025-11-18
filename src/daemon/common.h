#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <chrono>

/*
    Hard coded value so that there is no risk different machines having different naming
    (really unlikely scenario)
*/
// Every packet will be sent with a protocol that is meant for local development
constexpr int ETH_PROTOCOL = 0x88B5;  // ETH_P_802_EX1 in net/ethernet.h file
constexpr size_t DEVICE_EXP_MS = 30'000;
constexpr size_t HELLO_INTERVAL = 5'000;

inline int64_t get_curr_ms() {
    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return now_ms.count();
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
