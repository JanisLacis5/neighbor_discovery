#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include "types.h"

void create_frame(uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, EthFrame* dest);
void handle_frame(uint8_t* buf, ssize_t len);

#endif
