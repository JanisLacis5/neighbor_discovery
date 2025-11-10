#include <fcntl.h>
#include <net/ethernet.h>
#include <sys/epoll.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include "common.h"
#include "frame.h"
#include "net.h"
#include "types.h"

constexpr int MAX_EVENTS = 10;

GlobalData gdata;

int ifs_refresh() {
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;

    if (getifaddrs(&ifaddr) == -1) {
        printf("error\n");
        return -1;
    }

    // Add new interfaces, update existing ones. Skip non-ethernet ones
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        int family = ifa->ifa_addr->sa_family;
        if (ifa->ifa_addr == NULL || ifa->ifa_flags & IFF_LOOPBACK ||
            (!is_eth(ifa) && family != AF_INET && family != AF_INET6))  // not ethernet + no ip
            continue;

        int err = process_if(ifa);
        if (err) {
            printf("Error processing interface '%s'\n", ifa->ifa_name);
            freeifaddrs(ifaddr);
            return -1;
        }
    }

    freeifaddrs(ifaddr);
    return 0;
}

int scks_cleanup() {
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0)
        return -1;

    std::vector<int> todel;

    for (int idx = 0; idx < gdata.sockets.size(); idx++) {
        SocketInfo& sock_info = gdata.sockets[idx];
        if (curr_time - sock_info.last_seen_ms < 15'000)  // filter sockets that are idle for 15 seconds or more
            continue;

        // Remove socket from epoll
        if (epoll_ctl(gdata.epollfd, EPOLL_CTL_DEL, sock_info.fd, NULL) == -1) {
            printf("Error in epoll_ctl:del\n");
            return -1;
        }

        todel.push_back(idx);
    }
    for (int idx : todel) {
        int fd = gdata.sockets[idx].fd;
        close(fd);
        gdata.sockets[idx] = SocketInfo{};
    }
    return 0;
}

static int del_exp_devices() {
    std::vector<uint64_t> todel;
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0)
        return -1;

    for (auto& [dev_id, device] : gdata.store) {
        if (curr_time - device.last_seen_ms > 30'000 || device.interfaces.empty()) {
            todel.push_back(dev_id);
        }
    }

    for (uint64_t id : todel)
        gdata.store.erase(id);
    return 0;
}

static void del_if(uint8_t* ifa_name) {
    int ifa_idx = if_nametoindex((char*)ifa_name);

    // Delete info about the interface
    gdata.idx_to_info[ifa_idx] = InterfaceInfo{};

    // Close socket for this interface
}

static int del_exp_ifs() {
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0)
        return -1;

    for (auto& [dev_id, device] : gdata.store) {
        std::vector<int> if_todel;

        // Cast to interger to handle substraction below 0
        int ifs_size = (int)device.interfaces.size();

        /*
            Iterate in reverse order so that if_todel is descening and deleting is easier.
            Each delete after the first one would be invalid if if_todel wouldn't be descending because
            the position for every next element changes if one before it is deleted
        */
        for (int i = ifs_size - 1; i >= 0; i--) {
            InterfaceInfo if_info = gdata.idx_to_info[device.interfaces[i]];
            if (curr_time - if_info.last_seen_ms > 30'000) {
                del_if(if_info.if_name);
                if_todel.push_back(i);
            }
        }

        std::vector<int>::iterator devices_it = device.interfaces.begin();
        for (int i : if_todel)
            device.interfaces.erase(devices_it + i);
    }
    return 0;
}

int main() {
    // Set the device id
    int err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        printf("Error in random num generation\n");
        return -1;
    }

    // TODO: add some default sizes to gdata vectors

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        printf("Error in epoll_create\n");
        return -1;
    }
    gdata.epollfd = epollfd;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        err = ifs_refresh();
        if (err < 0) {
            printf("Error in the main loop\n");
            return -1;
        }

        err = scks_cleanup();
        if (err < 0) {
            printf("Error in the main loop\n");
            return -1;
        }

        int nfds = epoll_wait(gdata.epollfd, events, MAX_EVENTS, 1000);
        if (nfds == -1) {
            printf("Error in epoll_wait\n");
            return -1;
        }

        for (int i = 0; i < nfds; i++) {
            uint8_t buf[ETH_HLEN + ETH_PAYLOAD_LEN + 1];
            int fd = events[i].data.fd;
            ssize_t recvlen;

            while (true) {
                recvlen = recvfrom(fd, buf, sizeof(buf), 0, NULL, 0);

                if (recvlen == 0)
                    break;
                if (recvlen == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("Error reading socket");
                    break;
                }

                handle_frame(buf, recvlen);
            }
        }

        int64_t curr_time = get_curr_ms();
        for (int idx = 1; idx < gdata.sockets.size(); idx++) {
            SocketInfo& sock_info = gdata.sockets[idx];
            if (sock_info.fd == -1)
                continue;
            if (curr_time - sock_info.last_sent_ms < 5'000)
                continue;

            // Create and send the "HELLO" frame
            // EthFrame frame;
            // create_frame()
        }

        err = del_exp_devices();
        if (err < 0) {
            printf("Error in the main loop\n");
            return -1;
        }

        err = del_exp_ifs();
        if (err < 0) {
            printf("Error in the main loop\n");
            return -1;
        }
    }

    return 0;
}
