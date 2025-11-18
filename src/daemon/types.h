#ifndef TYPES_H
#define TYPES_H

#include <net/if.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

constexpr size_t MAGIC_LEN = 4;
constexpr size_t PROTOCOL_LEN = 2;
constexpr size_t MAC_LEN = 6;
constexpr size_t DEVICE_ID_LEN = 32;
constexpr size_t IPV4_LEN = 4;
constexpr size_t IPV6_LEN = 16;
constexpr size_t ETH_PAYLOAD_LEN = MAGIC_LEN + DEVICE_ID_LEN + IPV4_LEN + IPV6_LEN;

/* for struct Message, everything is an array of bytes to avoid endianness issues
 as this struct is sent over the network */
struct EthFrame {
    // HEADER (14 bytes)
    uint8_t dest_mac[MAC_LEN];
    uint8_t source_mac[MAC_LEN];
    uint16_t protocol;

    // PAYLOAD (56 bytes)
    uint8_t magic[MAGIC_LEN];          // MKTK
    uint8_t device_id[DEVICE_ID_LEN];  // ID of the sender
    uint8_t ipv4[IPV4_LEN];            // Sender's IPv4 on the interface
    uint8_t ipv6[IPV6_LEN];            // Sender's IPv6 on the interface
    // ip's are set to zeroes if they do not exist
};

struct IfaceInfo {
    // uint8_t iface_name[IF_NAMESIZE] = {0};
    uint8_t mac[MAC_LEN];
    uint8_t ipv4[IPV4_LEN] = {0};
    uint8_t ipv6[IPV6_LEN] = {0};
    uint64_t last_seen_ms = 0;  // timestamp when devices were connected on this specific interface
};

struct Device {
    uint64_t last_seen_ms;                      // timestamp when device was last seen on any interface
    std::unordered_map<int, IfaceInfo> ifaces;  // local iface idx : iface info on the neighbors machine
};

struct SocketInfo {
    int fd = -1;            // file descriptor
    uint64_t last_seen_ms;  // needed to close inactive connections
    uint64_t last_sent_ms;  // timestamp when the last 'hello' frame was sent
};

struct GlobalData {
    int epollfd;
    std::string device_id;
    std::unordered_map<std::string, Device> store;        // device id : device info
    std::vector<int> fd_to_iface{10, -1};                 // tells on which interface socket with `fd` is opened
    std::vector<IfaceInfo> idx_to_info{10, IfaceInfo{}};  // maps ifa_idx -> InterfaceInfo struct

    // interface index : socket info (socket open on interface whose idx is equal to the key)
    std::vector<SocketInfo> sockets{10, SocketInfo{}};
};
extern GlobalData gdata;

#endif
