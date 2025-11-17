#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "common.h"

bool all_zeroes(uint8_t buf[], uint8_t len) {
    for (int i = 0; i < len; i++) {
        if (buf[i] == 0)
            return false;
    }
    return true;
}

// Data is sent per neighbor - each send is sending everything
// about exactly one neighboring device
// TODO: optimize data streaming
/* Data is sent in a form of ... TODO - WRITE THE FORMAT HERE ...
    If val is empty, vallen=0 is still added because for each ethernet interface there are
    the same amount of tokens.*/
void cli_listall(int cli_fd) {
    // Process every neighbor, one message per neighbor
    // TODO: send message only with a full buffer
    if (gdata.store.empty()) {
        char buf[] = "No neighbors found\n";
        if (send(cli_fd, buf, sizeof(buf), 0) == -1)
            perror("cli_listall (empty)");

        return;
    }

    for (auto& [devid, device] : gdata.store) {
        uint64_t devid_netorder = htonll(devid);
        uint8_t id_bytes[8];
        for (int i = 0; i < 8; i++)
            id_bytes[i] = (devid_netorder >> (8 - i)) & 1;

        if (std::memcmp(id_bytes, gdata.device_id, 8) == 0)
            continue;

        char buf[8194];
        char* bufptr = buf;

        int n = std::sprintf(bufptr, "%02x%02x%02x%02x%02x%02x%02x%02x\n", id_bytes[0], id_bytes[1], id_bytes[2], id_bytes[3],
                     id_bytes[4], id_bytes[5], id_bytes[6], id_bytes[7]);
        bufptr += n;

        for (auto& [iface_idx, iface_info]: device.ifaces) {
            char iface_name[IF_NAMESIZE];
            char* res = if_indextoname(iface_idx, iface_name);
            if (res != iface_name) {
                perror("cli_listall (if_indextoname)");
                return;
            }

            n = std::sprintf(bufptr, "\t%s\n", iface_name);
            bufptr += n;

            const uint8_t* m = iface_info.mac;
            n = std::sprintf(bufptr, "\t\tMAC: %02x:%02x:%02x:%02x:%02x:%02x\n", m[0], m[1], m[2], m[3], m[4], m[5]);
            bufptr += n;

            if (all_zeroes(iface_info.ipv4, 4))
                n = std::sprintf(bufptr, "\t\tIPv4: NONE\n");
            else {
                const uint8_t* ip4 = iface_info.ipv4;
                n = std::sprintf(bufptr, "\t\tIPv4: %u.%u.%u.%u\n", ip4[0], ip4[1], ip4[2], ip4[3]);
            }
            bufptr += n;

            if (all_zeroes(iface_info.ipv6, 16))
                n = std::sprintf(bufptr, "\t\tIPv6: NONE\n");
            else {
                const uint8_t* ip6 = iface_info.ipv6;
                n = std::sprintf(bufptr, "\t\tIPv6: ");
                bufptr += n;

                for (int i = 0; i < 16; i += 2) {
                    n = std::sprintf(bufptr, "%02x%02x%s", ip6[i], ip6[i + 1], (i < 14 ? ":" : "\n"));
                    bufptr += n;
                }
            }
        }

        ssize_t len = bufptr - buf;
        if (len <= 0)
            continue;

        if (send(cli_fd, buf, len, 0) == -1)
            perror("cli_listall");
    }
}
