#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <cstring>
#include <cstdio>
#include "types.h"

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
    for (auto& [devid, device] : gdata.store) {
        if (std::memcmp(gdata.device_id, gdata.device_ids[devid], 8) == 0)
            continue;

        char buf[8194];
        char* bufptr = buf;

        // Device id as hex
        const uint8_t* id_bytes = gdata.device_ids[devid];
        std::sprintf(bufptr, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
            id_bytes[0], id_bytes[1], id_bytes[2], id_bytes[3],
            id_bytes[4], id_bytes[5], id_bytes[6], id_bytes[7]
        );
        bufptr += 8;

        for (int iface_idx : device.ifaces) {
            IfaceInfo& iface_info = gdata.idx_to_info[iface_idx];

            int n = std::sprintf(bufptr, "\t%s\n", iface_info.iface_name);
            bufptr += n;

            // MAC
            const uint8_t* m = iface_info.mac;
            std::sprintf(bufptr, "\tMAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                m[0], m[1], m[2], m[3], m[4], m[5]
            );
            bufptr += 6;

            if (all_zeroes(iface_info.ipv4, 4)) {
                n = std::sprintf(bufptr, "\tIPv4: NONE\n");
            } else {
                const uint8_t* ip4 = iface_info.ipv4;
                n = std::sprintf(bufptr, "\tIPv4: %u.%u.%u.%u\n",
                    ip4[0], ip4[1], ip4[2], ip4[3]
                );
            }
            bufptr += n;

            if (all_zeroes(iface_info.ipv6, 16)) {
                n = std::sprintf(bufptr, "\tIPv6: NONE\n");
            } else {
                const uint8_t* ip6 = iface_info.ipv6;
                n = std::sprintf(bufptr, "\tIPv6: ");
                bufptr += n;

                for (int i = 0; i < 16; i += 2) {
                    int m = std::sprintf(bufptr, "%02x%02x%s",
                        ip6[i], ip6[i+1], (i < 14 ? ":" : "\n")
                    );
                    bufptr += m;
                }
            }
        }

        size_t len = bufptr - buf;
        if (len == 0) continue;

        if (send(cli_fd, buf, len, 0) == -1) {
            perror("cli_listall");
        }
    }
}
