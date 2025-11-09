#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

enum ReturnType { SUCCESS = 0, ERROR = 1 };

/* for struct Message, everything is an array of bytes to avoid endianness issues
 as this struct is sent over the network */
struct Message {
    uint8_t magic[4];  // MKTK
    uint8_t device_id[8];
    uint8_t interface_name[IF_NAMESIZE];
    uint8_t mac[6];
    uint8_t pad[2];  // 2 byte padding because mac is 6 bytes
    uint8_t ipv4[4];
    uint8_t ipv6[16];
};

struct InterfaceInfo {
    uint8_t interface_name[IF_NAMESIZE];
    uint8_t mac[6];
    uint8_t pad[2];
    uint8_t ipv4[4];
    uint8_t ipv6[16];
    uint64_t last_seen_ms;  // timestamp when devices were connected on this specific interface
};

struct Device {
    uint64_t last_seen_ms;                  // timestamp when device was last seen on any interface
    std::vector<InterfaceInfo> interfaces;  // array of interfaces via 2 devices are connected
};

struct SocketInfo {
    int fd;                 // file descriptor
    uint64_t last_seen_ms;  // needed to close inactive connections
    uint64_t last_sent_ms;  // timestamp when the last 'hello' frame was sent
};

struct GlobalData {
    uint8_t device_id[8];                        // stored as byte array for easier Message building
    std::unordered_map<uint64_t, Device> store;  // device id : device info

    // interface index : socket info (socket open on interface whose idx is equal to the key)
    std::unordered_map<int, SocketInfo> sockets;
};

GlobalData gdata;

static uint64_t get_curr_ms() {
    struct timespec tp = {0, 0};

    int err = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (err) {
        printf("error in get_curr_ms\n");
        return ERROR;
    }

    return tp.tv_sec * 1000 + tp.tv_nsec / 1000 / 1000;
}

static void create_message(uint8_t* interface_name, uint8_t* mac, uint8_t* ipv4, uint8_t* ipv6, Message* dest) {
    Message message;

    memcpy(&message.magic, "MKTK", 4);
    memcpy(&message.device_id, &gdata.device_id, 8);
    memcpy(&message.interface_name, interface_name, IF_NAMESIZE);
    memcpy(&message.mac, mac, 6);
    memcpy(&message.ipv4, ipv4, 4);
    memcpy(&message.ipv6, ipv6, 16);

    *dest = message;
}

static bool is_eth(struct ifaddrs* ifa) {
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

int main() {
    // Set the device id
    ssize_t err = getrandom(&gdata.device_id, 8, GRND_RANDOM);
    if (err <= 0) {
        printf("Error in random num generation\n");
        return ERROR;
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
