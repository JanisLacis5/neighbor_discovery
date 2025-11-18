#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstring>
#include "types.h"

static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static bool all_zeroes(uint8_t buf[], uint8_t len) {
    for (int i = 0; i < len; i++) {
        if (buf[i] != 0)
            return false;
    }
    return true;
}

static void format_devid(uint64_t n, char out[27]) {
    char tmp[16];
    int i = 0;

    while (i < 13) {
        tmp[12 - i] = alphabet[n & 0x1F];
        n >>= 5;
        i++;
    }

    // Optional grouping: 4-4-4-1
    snprintf(out, 27, "%.4s-%.4s-%.4s-%c", tmp, tmp + 4, tmp + 8, tmp[12]);
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
        if (devid == gdata.device_id)
            continue;

        char buf[8194];
        char* bufptr = buf;

        char idstr[27];
        format_devid(devid, idstr);
        int n = std::sprintf(bufptr, "DEVICE ID: %s\n", idstr);
        bufptr += n;

        for (auto& [iface_idx, iface_info] : device.ifaces) {
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
                char ip4str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, iface_info.ipv4, ip4str, sizeof(ip4str));
                n = std::sprintf(bufptr, "\t\tIPv4: %s\n", ip4str);
            }
            bufptr += n;

            if (all_zeroes(iface_info.ipv6, 16))
                n = std::sprintf(bufptr, "\t\tIPv6: NONE\n");
            else {
                char ipv6str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, iface_info.ipv6, ipv6str, sizeof(ipv6str));
                n = std::sprintf(bufptr, "\t\tIPv6: %s\n", ipv6str);
                bufptr += n;
            }
        }

        ssize_t len = bufptr - buf;
        if (len <= 0)
            continue;

        if (send(cli_fd, buf, len, 0) == -1)
            perror("cli_listall");
    }
}
