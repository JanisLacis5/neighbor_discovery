#include "frame.h"
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <cstring>
#include "common.h"
#include "types.h"

static int unpack_frame(uint8_t* buf, ssize_t len, EthFrame* dest) {
    if (len < ETH_HLEN + ETH_PAYLOAD_LEN)
        return -1;

    // Read and check the magic string
    std::memcpy(dest->magic, buf + ETH_HLEN, MAGIC_LEN);
    if (std::memcmp((char*)&dest->magic, "MKTK", MAGIC_LEN))  // Not our packet
        return -1;

    std::memcpy(dest->dest_mac, buf, MAC_LEN);
    buf += 6;
    std::memcpy(dest->source_mac, buf, MAC_LEN);
    buf += 6 + 2 + 4;  // we know the protocol already, magic copied already
    std::memcpy(dest->device_id, buf, DEVICE_ID_LEN);
    buf += 8;
    std::memcpy(dest->ipv4, buf, IPV4_LEN);
    buf += 4;
    std::memcpy(dest->ipv6, buf, IPV6_LEN);

    return 0;
}

static void create_frame(const uint8_t* ipv4, const uint8_t* ipv6, const uint8_t* mac, EthFrame* dest) {
    // Set the header
    std::memset(dest->dest_mac, 0xff, MAC_LEN);
    std::memcpy(dest->source_mac, mac, MAC_LEN);
    dest->protocol = htons(ETH_PROTOCOL);

    // Set the payload
    std::memcpy(dest->magic, "MKTK", MAGIC_LEN);

    uint64_t devid_netw = htonll(gdata.device_id);
    std::memcpy(dest->device_id, &devid_netw, DEVICE_ID_LEN);

    if (!ipv4)
        std::memset(dest->ipv4, 0, IPV4_LEN);
    else
        std::memcpy(dest->ipv4, ipv4, IPV4_LEN);

    if (!ipv6)
        std::memset(dest->ipv6, 0, IPV6_LEN);
    else
        std::memcpy(dest->ipv6, ipv6, IPV6_LEN);
}

void handle_frame(int iface_idx, uint8_t* buf, ssize_t len) {
    struct EthFrame frame;
    if (unpack_frame(buf, len, &frame) == -1)
        return;

    int64_t curr_time = get_curr_ms();
    if (curr_time == -1)
        return;

    uint64_t sender_device_id;
    std::memcpy(&sender_device_id, frame.device_id, DEVICE_ID_LEN);
    sender_device_id = ntohll(sender_device_id);

    if (sender_device_id == gdata.device_id)
        return;

    Device& device = gdata.store[sender_device_id];
    device.last_seen_ms = curr_time;

    IfaceInfo& iface = device.ifaces[iface_idx];
    iface.last_seen_ms = curr_time;
    std::memcpy(iface.mac, frame.source_mac, MAC_LEN);
    std::memcpy(iface.ipv4, frame.ipv4, IPV4_LEN);
    std::memcpy(iface.ipv6, frame.ipv6, IPV6_LEN);
}

static void pack_frame(EthFrame& frame, uint8_t* buf) {
    uint8_t offset = 0;

    std::memcpy(buf + offset, frame.dest_mac, MAC_LEN);
    offset += 6;
    std::memcpy(buf + offset, frame.source_mac, MAC_LEN);
    offset += 6;
    std::memcpy(buf + offset, &frame.protocol, PROTOCOL_LEN);
    offset += 2;
    std::memcpy(buf + offset, frame.magic, MAC_LEN);
    offset += 4;
    std::memcpy(buf + offset, frame.device_id, DEVICE_ID_LEN);
    offset += 8;
    std::memcpy(buf + offset, frame.ipv4, IPV4_LEN);
    offset += 4;
    std::memcpy(buf + offset, frame.ipv6, IPV6_LEN);
}

static void send_frame(int fd, int iface_idx, uint8_t* buf, size_t len, uint8_t* dest_mac) {
    struct sockaddr_ll addr;

    addr.sll_ifindex = iface_idx;
    addr.sll_halen = ETH_ALEN;
    std::memcpy(addr.sll_addr, dest_mac, ETH_ALEN);

    if (sendto(fd, buf, len, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr_ll)) < 0)
        perror("send_hello");
}

void send_hello(int fd, int iface_idx, const uint8_t* ipv4, const uint8_t* ipv6, const uint8_t* source_mac) {
    EthFrame frame;
    create_frame(ipv4, ipv6, source_mac, &frame);

    uint8_t buf[ETH_HLEN + ETH_PAYLOAD_LEN];
    pack_frame(frame, buf);

    send_frame(fd, iface_idx, buf, sizeof(buf), frame.dest_mac);
}
