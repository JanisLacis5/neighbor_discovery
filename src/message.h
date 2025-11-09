#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include "types.h"

void create_message(uint8_t* interface_name, uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, Message* dest);

#endif
