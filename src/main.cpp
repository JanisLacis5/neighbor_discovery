#include <sys/random.h>
#include "common.h"
#include "types.h"
#include "net.h"

GlobalData gdata;

int main() {
    // Set the device id
    ssize_t err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        printf("Error in random num generation\n");
        return 1;
    }

    while (true) {
        // Iterate over each interface
        struct ifaddrs* ifaddr;
        struct ifaddrs* ifa;

        if (getifaddrs(&ifaddr) == -1) {
            printf("error\n");
            return -1;
        }

        // Add new interfaces, update existing ones
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || ifa->ifa_flags & IFF_LOOPBACK)
                continue;

            std::vector<int> interfaces;
            uint64_t curr_time = get_curr_ms();

            if (is_eth(ifa)) {
                int ifa_idx = if_nametoindex(ifa->ifa_name);

                if (gdata.sockets.find(ifa_idx) != gdata.sockets.end())
                    gdata.sockets[ifa_idx].last_seen_ms = curr_time;
                else {
                    int fd;  // TODO: open a new socket on this interface
                    struct SocketInfo sock_info;
                    sock_info.fd = fd;
                    sock_info.last_seen_ms = curr_time;
                    sock_info.last_sent_ms = 0;
                    gdata.sockets[ifa_idx] = {fd, curr_time, curr_time};
                }
            }
        }

        // Clean up old interfaces
        int curr_time = get_curr_ms();
        for (auto& [idx, fd_info] : gdata.sockets) {
            if (fd_info.last_seen_ms - curr_time < 15'000)  // filter sockets that are idle for 15 seconds or more
                continue;

            gdata.sockets.erase(idx);
        }

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

        // Erase expired devices
        curr_time = get_curr_ms();
        std::vector<uint64_t> devices_todel;
        for (auto& [dev_id, device] : gdata.store) {
            if (curr_time - device.last_seen_ms > 30'000 || device.interfaces.empty()) {
                devices_todel.push_back(dev_id);
                continue;
            }

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

        // Delete old devices
        for (uint64_t id : devices_todel)
            gdata.store.erase(id);
    }

    return 0;
}
