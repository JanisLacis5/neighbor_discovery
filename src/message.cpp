#include "message.h"
#include <cstring>

void create_message(uint8_t* interface_name, uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, Message* dest) {
    Message message;

    memcpy(&message.magic, "MKTK", 4);
    memcpy(&message.device_id, &gdata.device_id, 8);
    memcpy(&message.interface_name, interface_name, IF_NAMESIZE);
    memcpy(&message.mac, mac, 6);
    memcpy(&message.ipv4, ipv4, 4);
    memcpy(&message.ipv6, ipv6, 16);

    *dest = message;
}
