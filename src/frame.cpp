#include "frame.h"
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <cstring>
#include "common.h"

static int format_frame(uint8_t* buf, ssize_t len, EthFrame* dest) {
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

void create_frame(uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, EthFrame* dest) {
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

void handle_frame(uint8_t* buf, ssize_t len) {
    // Read the frame into EthFrame struct
    struct EthFrame frame;
    if (format_frame(buf, len, &frame) == -1)
        return;

    // TODO: handle the frame
}
