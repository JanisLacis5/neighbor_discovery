#include <net/if.h>
#include <cstring>
#include "types.h"

// Data is sent per neighbor - each send is sending everything
// about exactly one neighboring device

/* Data is sent in a form of ... TODO - WRITE THE FORMAT HERE ...
    If val is empty, vallen=0 is still added because for each ethernet interface there are
    the same amount of tokens.*/
void cli_listall() {
    // Process every neighbor, one message per neighbor
    for (auto& [devid, device] : gdata.store) {
        uint8_t buf[8192];
        uint8_t* bufptr = buf;
        uint64_t device_id;
        std::memcpy(&device_id, gdata.device_id, 8);

        if (devid == device_id)
            continue;

        std::memcpy(bufptr, gdata.device_ids[devid], 8);  // Add the neighbors id

        // Add interface count between the device pair is connected via
        int32_t ifaces_cnt = device.ifaces.size();
        std::memcpy(bufptr, &ifaces_cnt, 4);

        // Write all information about interfaces
        for (int iface_idx : device.ifaces) {
            IfaceInfo& iface_info = gdata.idx_to_info[iface_idx];

            std::memcpy(bufptr, iface_info.iface_name, IF_NAMESIZE);
            bufptr += IF_NAMESIZE;
            std::memcpy(bufptr, iface_info.mac, 6);
            bufptr += 6;
            std::memcpy(bufptr, iface_info.ipv4, 4);
            bufptr += 4;
            std::memcpy(bufptr, iface_info.ipv4, 16);
            bufptr += 16;
        }

        // Send data to the cli
    }
}
