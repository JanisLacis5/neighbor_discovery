#include <ifaddrs.h>
#include <netdb.h>
#include <unordered_map>
#include "net/if.h"

struct Message {
    uint8_t magic[4];  // MKTK
    uint64_t device_id;
    uint32_t interface_name[IF_NAMESIZE];
    uint64_t mac;  // 2 msb are 0
    uint32_t ipv4;
    uint8_t ipv6[16];
};

struct Device {};

struct GlobalData {
    uint64_t device_id;
    std::unordered_map<uint64_t, Device> store;
};

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
