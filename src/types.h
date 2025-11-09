#ifndef TYPES_H
#define TYPES_H

#include <net/if.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

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

struct SocketInfo {
    int fd;                 // file descriptor
    uint64_t last_seen_ms;  // needed to close inactive connections
    uint64_t last_sent_ms;  // timestamp when the last 'hello' frame was sent
};

struct GlobalData {
    int epollfd;
    uint8_t device_id[8];                        // stored as byte array for easier Message building
    std::unordered_map<uint64_t, Device> store;  // device id : device info

    // interface index : socket info (socket open on interface whose idx is equal to the key)
    std::unordered_map<int, SocketInfo> sockets;
};
extern GlobalData gdata;

#endif
