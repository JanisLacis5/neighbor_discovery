#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include "net/if.h"
#include <unordered_map>
#include <vector>

struct Message {
    uint8_t magic[4];  // MKTK
    uint64_t device_id;
    uint32_t interface_name[IF_NAMESIZE];
    uint64_t mac;  // 2 msb are padding (set as 0)
    uint32_t ipv4;
    uint8_t ipv6[16];
};

struct InterfaceInfo {
    uint32_t interface_name[IF_NAMESIZE];
    uint64_t mac;  // 2 msb are padding
    uint32_t ipv4;
    uint8_t ipv6;
    uint64_t last_seen_ms;  // timestamp when devices were connected on this specific interface
};

struct Device {
    uint64_t last_seen_ms;  // timestamp when device was last seen on any interface
    std::vector<InterfaceInfo> interfaces;  // array of interfaces via 2 devices are connected
};

struct GlobalData {
    uint64_t device_id;
    std::unordered_map<uint64_t, Device> store; // device id : device info
};

uint64_t get_curr_ms() {
    struct timespec tp = {0, 0};

    int err = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (err) {
        printf("error in get_curr_ms\n");
        return 0;
    }

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000;
}

int main() {
    return 0;
}

// int main() {
//     struct ifaddrs* ifaddr;
//     char host[NI_MAXHOST];
//
//     if (getifaddrs(&ifaddr) == -1) {
//         printf("error\n");
//         return -1;
//     }
//
//     while (ifaddr != NULL) {
//         if (ifaddr->ifa_addr == NULL)
//             continue;
//
//         int family = ifaddr->ifa_addr->sa_family;
//
//         ifaddr = ifaddr->ifa_next;
//     }
//     return 0;
// }
