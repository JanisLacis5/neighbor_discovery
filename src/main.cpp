#include <sys/random.h>
#include <vector>
#include "common.h"
#include "net.h"
#include "types.h"

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
        process_eth(ifa);
    }

    freeifaddrs(ifaddr);
    return 0;
}

void scks_cleanup() {
    int curr_time = get_curr_ms();
    std::vector<int> todel;

    for (auto& [idx, fd_info] : gdata.sockets) {
        if (fd_info.last_seen_ms - curr_time < 15'000)  // filter sockets that are idle for 15 seconds or more
            continue;
        todel.push_back(idx);
    }

    for (int idx : todel)
        gdata.sockets.erase(idx);
}

static void del_exp_devices() {
    std::vector<uint64_t> todel;
    uint64_t curr_time = get_curr_ms();

    for (auto& [dev_id, device] : gdata.store) {
        if (curr_time - device.last_seen_ms > 30'000 || device.interfaces.empty()) {
            todel.push_back(dev_id);
        }
    }

    // Delete old devices
    for (uint64_t id : todel)
        gdata.store.erase(id);
}

static void del_exp_ifs() {
    uint64_t curr_time = get_curr_ms();

    for (auto& [dev_id, device] : gdata.store) {
        std::vector<int> if_todel;

        // Iterate in reverse order so that if_todel is descening and deleting is easier
        for (int i = (int)device.interfaces.size() - 1; i >= 0; i--) {
            InterfaceInfo if_info = device.interfaces[i];
            if (curr_time - if_info.last_seen_ms > 30'000)
                if_todel.push_back(i);
        }

        // Delete old interfaces
        std::vector<InterfaceInfo>::iterator devices_it = device.interfaces.begin();
        for (int i : if_todel)
            device.interfaces.erase(devices_it + i);
    }
}

int main() {
    // Set the device id
    ssize_t err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        printf("Error in random num generation\n");
        return 1;
    }

    while (true) {
        ifs_refresh();
        scks_cleanup();

        // TODO: see which sockets are ready to read from (from those interfaces that the host machine are conneced to)
        /*
            use epoll() to see which sockets i can read from and iterate over those and read
            for socket in read_ready:
                Message messsage = read(socket)
                if message.device_id == gdata.device_id:  // ignore myself
                    continue;
                update_store(message)
        */

        // TODO: brodcast a 'hello' frame as the host machine to everyone on each socket
        /*
            for _, [fd, _, last_sent] in gdata.sockets:
                if curr_time - last_sent_ms < 5sec:
                    continue
                source = curr_socket_mac
                dest = ff:ff:ff:ff:ff:ff  // each fd is bound to only one eth interface so this is fine
                send(source, dest, struct Message)
        */

        del_exp_devices();
        del_exp_ifs();
    }

    return 0;
}
