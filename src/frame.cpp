#include "frame.h"
#include <net/ethernet.h>
#include <netinet/in.h>
#include <cstring>
#include "common.h"

void create_frame(uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, EthFrame* dest) {
    // Set the header
    std::memset(dest->dest_mac, 0xff, 6);
    std::memcpy(dest->source_mac, mac, 6);
    dest->protocol = htons(ETH_PROTOCOL);

    // Set the payload
    std::memcpy(dest->magic, "MKTK", 4);
    std::memcpy(dest->device_id, gdata.device_id, 8);
    std::memcpy(dest->ipv4, ipv4, 4);
    std::memcpy(dest->ipv6, ipv6, 16);
}
