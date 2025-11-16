#ifndef IFACES_H
#define IFACES_H

#include <ifaddrs.h>

bool is_eth(struct ifaddrs* ifa);
int process_iface(struct ifaddrs* ifa);
int ifaces_refresh();
int del_exp_ifaces();

#endif
