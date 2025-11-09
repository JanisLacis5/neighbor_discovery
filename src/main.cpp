#include <sys/random.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
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
        if (ifa->ifa_addr == NULL || ifa->ifa_flags & IFF_LOOPBACK || !is_eth(ifa))
            continue;
        int err = process_eth(ifa);
        if (err) {
            printf("Error processing interface '%s'\n", ifa->ifa_name);
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

    for (auto& [idx, fd_info] : gdata.sockets) {
        if (curr_time - fd_info.last_seen_ms < 15'000)  // filter sockets that are idle for 15 seconds or more
            continue;
        
        // Remove socket from epoll
        if (epoll_ctl(gdata.epollfd, EPOLL_CTL_DEL, fd_info.fd, NULL) == -1) {
            printf("Error in epoll_ctl:del\n");
            return -1;
        }

        todel.push_back(idx);
   }
    for (int idx : todel) {
        int fd = gdata.sockets[idx].fd;
        close(fd);
        gdata.sockets.erase(idx);
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

static int del_exp_ifs() {
    uint64_t curr_time = get_curr_ms();
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
            InterfaceInfo if_info = device.interfaces[i];
            if (curr_time - if_info.last_seen_ms > 30'000)
                if_todel.push_back(i);
        }

        std::vector<InterfaceInfo>::iterator devices_it = device.interfaces.begin();
        for (int i : if_todel)
            device.interfaces.erase(devices_it + i);
    }
    return 0;
}

static int setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        printf("Error in flags/setnonblocking\n");
        return -1;
    }

    int err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (err == -1) {
        printf("Error in setnonblocking\n");
        return -1;
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
    
    int epollfd = epoll_create(1);
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
            // TODO: handle socket reading here
        }


        // TODO: brodcast a 'hello' frame as the host machine to everyone on each socket
        /*
            for _, [fd, _, last_sent] in gdata.sockets:
                if curr_time - last_sent_ms < 5sec:
                    continue
                source = curr_socket_mac
                dest = ff:ff:ff:ff:ff:ff  // each fd is bound to only one eth interface so this is fine
                send(source, dest, struct Message)
        */

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
