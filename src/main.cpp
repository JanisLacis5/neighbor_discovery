#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

enum ReturnType { SUCCESS = 0, ERROR = 1 };

/* for struct Message, everything is an array of bytes to avoid endianness issues
 as this struct is sent over the network */
struct Message {
    uint8_t magic[4];  // MKTK
    uint8_t device_id[8];
    uint8_t interface_name[IF_NAMESIZE];
    uint8_t mac[6];
    uint8_t pad[2];  // 2 byte padding because mac is 6 bytes
    uint8_t ipv4[4];
    uint8_t ipv6[16];
};

struct InterfaceInfo {
    uint8_t interface_name[IF_NAMESIZE];
    uint8_t mac[6];
    uint8_t pad[2];
    uint8_t ipv4[4];
    uint8_t ipv6[16];
    uint64_t last_seen_ms;  // timestamp when devices were connected on this specific interface
};

struct Device {
    uint64_t last_seen_ms;                  // timestamp when device was last seen on any interface
    std::vector<InterfaceInfo> interfaces;  // array of interfaces via 2 devices are connected
};

struct GlobalData {
    uint8_t device_id[8];                        // stored as byte array for easier Message building
    std::unordered_map<uint64_t, Device> store;  // device id : device info
};

GlobalData gdata;

static uint64_t get_curr_ms() {
    struct timespec tp = {0, 0};

    int err = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (err) {
        printf("error in get_curr_ms\n");
        return ERROR;
    }

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000;
}

static void create_message(uint8_t* interface_name, uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, Message* dest) {
    Message message;

    memcpy(&message.magic, "MKTK", 4);
    memcpy(&message.device_id, &gdata.device_id, 8);
    memcpy(&message.interface_name, interface_name, IF_NAMESIZE);
    memcpy(&message.mac, mac, 6);
    memcpy(&message.ipv4, ipv4, 4);
    memcpy(&message.ipv6, ipv6, 16);

    *dest = message;
}

static int check_wireless(const char* ifname) {
    char path[256] = {};
    snprintf(path, 255, "/sys/class/net/%s/wireless", ifname);
    return access(path, F_OK) == 0;
}

int main() {
    // Set the device id
    ssize_t err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        printf("Error in random num generation\n");
        return ERROR;
    }

    // Iterate over each interface
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;

    if (getifaddrs(&ifaddr) == -1) {
        printf("error\n");
        return -1;
    }

    // for more information on interface detection, see man getifaddrs example at the bottom
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_flags & IFF_LOOPBACK)
            continue;

        // Find all ethernet interfaces
        int family = ifa->ifa_addr->sa_family;
        struct sockaddr_ll* flags = (struct sockaddr_ll*)ifa->ifa_addr;
        bool is_eth = (family == AF_PACKET && flags->sll_hatype == ARPHRD_ETHER && !check_wireless(ifa->ifa_name));

        if (is_eth) {
            printf("%s  address family: %d%s\n", ifa->ifa_name, family,
                   (family == AF_PACKET)  ? " (AF_PACKET)"
                   : (family == AF_INET)  ? " (AF_INET)"
                   : (family == AF_INET6) ? " (AF_INET6)"
                                          : "");
            printf("hardware type is ARPHRD_ETHER?: %d\n", flags->sll_hatype == ARPHRD_ETHER);
        }
    }

    return 0;
}
