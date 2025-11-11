#include <sys/epoll.h>
#include <sys/random.h>
#include <cerrno>
#include "common.h"
#include "frame.h"
#include "ifaces.h"
#include "sockets.h"
#include "types.h"

constexpr int MAX_EVENTS = 10;
GlobalData gdata;

static int del_exp_devices() {
    std::vector<uint64_t> todel;
    int64_t curr_time = get_curr_ms();
    if (curr_time < 0) {
        perror("del_exp_devices");
        return -1;
    }

    for (auto& [dev_id, device] : gdata.store) {
        if (curr_time - device.last_seen_ms > 30'000 || device.ifaces.empty()) {
            todel.push_back(dev_id);
        }
    }

    for (uint64_t id : todel)
        gdata.store.erase(id);
    return 0;
}

int main() {
    // Set the device id
    int err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        perror("main (random num generation)");
        return -1;
    }

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("main (epoll_create");
        return -1;
    }
    gdata.epollfd = epollfd;
    struct epoll_event events[MAX_EVENTS];

    while (true) {
        err = ifaces_refresh();
        if (err < 0)
            return -1;

        err = scks_cleanup();
        if (err < 0)
            return -1;

        int nfds = epoll_wait(gdata.epollfd, events, MAX_EVENTS, 1000);
        if (nfds == -1) {
            perror("main (epoll_wait)");
            return -1;
        }

        for (int i = 0; i < nfds; i++) {
            uint8_t buf[1500];
            int fd = events[i].data.fd;
            ssize_t recvlen;

            while (true) {
                recvlen = recvfrom(fd, buf, sizeof(buf), 0, NULL, 0);

                if (recvlen == 0)
                    break;
                if (recvlen == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK)
                        perror("main (reading socket)");
                    break;
                }

                int iface_idx = gdata.fd_to_iface[fd];
                handle_frame(iface_idx, buf, recvlen);
            }
        }

        int64_t curr_time = get_curr_ms();
        for (int idx = 1; idx < gdata.sockets.size(); idx++) {
            SocketInfo& sock_info = gdata.sockets[idx];
            const IfaceInfo& iface_info = gdata.idx_to_info[idx];

            if (sock_info.fd == -1)
                continue;
            if (curr_time - sock_info.last_sent_ms < 5'000)
                continue;

            // DEBUG
            printf("curr time: %ld\n", curr_time);
            printf("last sent: %ld\n", sock_info.last_sent_ms);

            send_hello(sock_info.fd, idx, iface_info.ipv4, iface_info.ipv6, iface_info.mac);
            sock_info.last_sent_ms = curr_time;
        }

        err = del_exp_devices();
        if (err < 0)
            return -1;

        err = del_exp_ifaces();
        if (err < 0)
            return -1;
    }

    return 0;
}
