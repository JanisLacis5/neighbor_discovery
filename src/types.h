#ifndef TYPES_H
#define TYPES_H

#include <net/if.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#define ETH_PAYLOAD_LEN 32

/* for struct Message, everything is an array of bytes to avoid endianness issues
 as this struct is sent over the network */
struct EthFrame {
    // HEADER (14 bytes)
    uint8_t dest_mac[6];
    uint8_t source_mac[6];
    uint16_t protocol;

    // PAYLOAD (32 bytes)
    uint8_t magic[4];      // MKTK
    uint8_t device_id[8];  // ID of the sender
    uint8_t ipv4[4];       // Sender's IPv4 on the interface
    uint8_t ipv6[16];      // Sender's IPv6 on the interface
    // ip's are set to zeroes if they do not exist
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
