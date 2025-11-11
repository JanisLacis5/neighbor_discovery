#include "ifaces.h"
#include <ifaddrs.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include "common.h"
#include "sockets.h"
#include "types.h"

static bool all_zero(uint8_t* num, size_t len) {
    return std::all_of(num, num + len, [](uint8_t x) { return x == 0; });
}

static void fill_info_raw(struct ifaddrs* ifa, IfaceInfo& iface_info) {
    struct sockaddr_ll* sll = (struct sockaddr_ll*)ifa->ifa_addr;
    std::strncpy((char*)iface_info.iface_name, ifa->ifa_name, IF_NAMESIZE);
    std::memcpy(iface_info.mac, sll->sll_addr, sll->sll_halen);
    iface_info.is_init = true;
}

static void fill_info_ip4(struct ifaddrs* ifa, IfaceInfo& iface_info) {
    struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
    std::memcpy(iface_info.ipv4, &sin->sin_addr.s_addr, 4);
}

static void fill_info_ip6(struct ifaddrs* ifa, IfaceInfo& iface_info) {
    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)ifa->ifa_addr;
    std::memcpy(iface_info.ipv6, sin6->sin6_addr.s6_addr, 16);
}

static int update_iface_info(struct ifaddrs* ifa) {
    int64_t curr_time = get_curr_ms();
    int family = ifa->ifa_addr->sa_family;

    int ifa_idx = if_nametoindex(ifa->ifa_name);
    if (ifa_idx == 0) {
        perror("update_iface_info");
        return -1;
    }

    if (gdata.idx_to_info.size() <= ifa_idx)
        gdata.idx_to_info.resize(ifa_idx + 1);

    struct IfaceInfo& iface_info = gdata.idx_to_info[ifa_idx];
    iface_info.last_seen_ms = curr_time;

    if (family == AF_PACKET && !iface_info.is_init)
        fill_info_raw(ifa, iface_info);
    else if (family == AF_INET && all_zero(&iface_info.ipv4[0], 4))
        fill_info_ip4(ifa, iface_info);
    else if (family == AF_INET6 && all_zero(&iface_info.ipv6[0], 16))
        fill_info_ip6(ifa, iface_info);
    else
        // This function should only receive AF_PACKET, AF_INET or AF_INET6 interfaces
        return -1;

    return 0;
}

bool is_eth(struct ifaddrs* ifa) {
    int family = ifa->ifa_addr->sa_family;
    if (family != AF_PACKET)
        return false;

    struct sockaddr_ll* flags = (struct sockaddr_ll*)ifa->ifa_addr;
    if (flags->sll_hatype != ARPHRD_ETHER)
        return false;

    char pathw[256] = {};
    char pathd[256] = {};
    snprintf(pathw, 255, "/sys/class/net/%s/wireless", ifa->ifa_name);

    // https://www.ibm.com/docs/en/linux-on-systems?topic=ni-matching-devices-1
    snprintf(pathd, 255, "/sys/class/net/%s/device",
             ifa->ifa_name);  // TODO: test this one (should check if device is not virtual)

    return access(pathw, F_OK) != 0 && access(pathd, F_OK) == 0;
}

int process_iface(struct ifaddrs* ifa) {
    int ifa_idx = if_nametoindex(ifa->ifa_name);
    int64_t curr_time = get_curr_ms();

    // Update interface info
    if (update_iface_info(ifa) == -1)
        return -1;

    // Update socket info for the interface
    if (gdata.sockets[ifa_idx].fd != -1)
        gdata.sockets[ifa_idx].last_seen_ms = curr_time;
    else {
        int fd = open_socket(ifa_idx);
        if (fd == -1)
            return -1;

        if (gdata.sockets.size() <= ifa_idx)
            gdata.sockets.resize(ifa_idx + 1);
        struct SocketInfo& sock_info = gdata.sockets[ifa_idx];
        sock_info.fd = fd;
        sock_info.last_seen_ms = curr_time;
        sock_info.last_sent_ms = 0;
    }

    return 0;
}

static bool is_good_iface(struct ifaddrs* ifa) {
    int family = ifa->ifa_addr->sa_family;

    if (ifa->ifa_addr == NULL || ifa->ifa_flags & IFF_LOOPBACK)
        return false;
    if (!is_eth(ifa) && family != AF_INET && family != AF_INET6)  // not ethernet + no ip
        return false;
    return true;
}

int ifaces_refresh() {
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("ifaces_refresh (getifaddrs)");
        return -1;
    }

    // Add new interfaces, update existing ones. Skip non-ethernet ones
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!is_good_iface(ifa))
            continue;

        int err = process_iface(ifa);
        if (err) {
            perror("ifaces_refresh (loop)");
            freeifaddrs(ifaddr);
            return -1;
        }
    }

    freeifaddrs(ifaddr);
    return 0;
}

static void del_iface(int iface_idx) {
    gdata.idx_to_info[iface_idx] = IfaceInfo{};
    close_sock(iface_idx);
}

static void process_exp_iface(struct Device& device, std::vector<int>& ifaces_todel, int64_t curr_time) {
    for (int iface_idx : device.ifaces) {
        const IfaceInfo& iface_info = gdata.idx_to_info[iface_idx];

        if (curr_time - iface_info.last_seen_ms > 30'000) {
            del_iface(iface_idx);
            ifaces_todel.push_back(iface_idx);
        }
    }
}

int del_exp_ifaces() {
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0) {
        perror("del_exp_ifaces");
        return -1;
    }

    for (auto& [dev_id, device] : gdata.store) {
        std::vector<int> ifaces_todel;
        process_exp_iface(device, ifaces_todel, curr_time);

        for (int i : ifaces_todel)
            device.ifaces.erase(i);
    }
    return 0;
}
