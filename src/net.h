#ifndef NET_H
#define NET_H

#include <ifaddrs.h>

bool is_eth(struct ifaddrs* ifa);
int process_if(struct ifaddrs* ifa);

#endif
