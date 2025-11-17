#include "frame.h"
#include <linux/if_ether.h>
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
    std::memcpy(dest->magic, buf + ETH_HLEN, 4);
    if (std::memcmp((char*)&dest->magic, "MKTK", 4))  // Not our packet
        return -1;

    std::memcpy(dest->dest_mac, buf, 6);
    buf += 6;
    std::memcpy(dest->source_mac, buf, 6);
    buf += 6 + 2 + 4;  // we know the protocol already, magic copied already
    std::memcpy(dest->device_id, buf, 8);
    buf += 8;
    std::memcpy(dest->ipv4, buf, 4);
    buf += 4;
    std::memcpy(dest->ipv6, buf, 16);

    return 0;
}

static void create_frame(const uint8_t* ipv4, const uint8_t* ipv6, const uint8_t* mac, EthFrame* dest) {
    // Set the header
    std::memset(dest->dest_mac, 0xff, 6);
    std::memcpy(dest->source_mac, mac, 6);
    dest->protocol = htons(ETH_PROTOCOL);

    // Set the payload
    std::memcpy(dest->magic, "MKTK", 4);
    std::memcpy(dest->device_id, gdata.device_id, 8);

    if (!ipv4)
        std::memset(dest->ipv4, 0, 4);
    else
        std::memcpy(dest->ipv4, ipv4, 4);

    if (!ipv6)
        std::memset(dest->ipv6, 0, 16);
    else
        std::memcpy(dest->ipv6, ipv6, 16);
}

void handle_frame(int iface_idx, uint8_t* buf, ssize_t len) {
    struct EthFrame frame;
    if (unpack_frame(buf, len, &frame) == -1)
        return;

    int64_t curr_time = get_curr_ms();
    if (curr_time == -1)
        return;

    uint64_t sender_device_id;
    std::memcpy(&sender_device_id, frame.device_id, 8);
    sender_device_id = ntohll(sender_device_id);

    if (std::memcmp(frame.device_id, gdata.device_id, 8) == 0)
        return;

    gdata.store[sender_device_id].last_seen_ms = curr_time;
    gdata.store[sender_device_id].ifaces.insert(iface_idx);

    // DEBUG
    printf("received frame from %ld, its mac address is ", sender_device_id);
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n", frame.source_mac[0], frame.source_mac[1], frame.source_mac[2],
           frame.source_mac[3], frame.source_mac[4], frame.source_mac[5]);
}

static void pack_frame(EthFrame& frame, uint8_t* buf) {
    uint8_t offset = 0;

    std::memcpy(buf + offset, frame.dest_mac, 6);
    offset += 6;
    std::memcpy(buf + offset, frame.source_mac, 6);
    offset += 6;
    std::memcpy(buf + offset, &frame.protocol, 2);
    offset += 2;
    std::memcpy(buf + offset, frame.magic, 4);
    offset += 4;
    std::memcpy(buf + offset, frame.device_id, 8);
    offset += 8;
    std::memcpy(buf + offset, frame.ipv4, 4);
    offset += 4;
    std::memcpy(buf + offset, frame.ipv6, 16);
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
