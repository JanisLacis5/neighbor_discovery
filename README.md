# Neighbor discovery service
## Overview
This neighbor discovery service program consists of 2 seperate programs: daemon and CLI.
1. Daemon
The daemon is a service that is running in the background. It:
- Listens on all interfaces that the host machine has
- Every 5 seconds it sends a "hello" ethernet frame.
- Recieves frames from other machines connected to the same ethernet service that are running the same daemon
- Maintains a list of neighbors and their information on each interface

2. CLI
The CLI tool is a program that communicates with the daemon to retrieve information about neighbors. It:
- Has a command `list` which returns each neighbor and all interfaces via the host and the neighbor are connected
- Displays this information in human-readable format

## Architecture
### Daemon
The daemon performs the following tasks:

1. Interface discovery
- Enumerates all host's interfaces 
- Filters out only ethernet interfaces
- Excludes wireless and virtual interfaces that may pass ethernet checking
- Tracks interfaces dynamically (sockets are opened / closed while running based on idle time)

2. Frame Transmission
- Builds and transmits a custom ethernet frame containing:
    - sender MAC
    - magic signature (MKTK)
    - 32-byte device ID from `/etc/machine-id`
    - IPv4 (if available)
    - IPv6 (if available)

3. Frame Reception
- Listens via sockets bound to each interface
- Validates incoming frames (magic header + payload length)
- Updates neighbor state (per device, per interface)

4. Neighbor Table Maintenance
- Stores neighbors in a hash map: `unordered_map<string, Device>`
- Tracks last seen timestamp per interface
- Removes neighbors with no active interfaces for over 30 seconds

5. CLI Communication
- Provides a UNIX domain (local) socket for the CLI
- Returns the neighbor table on request

### CLI
The CLI:
- Connects to daemon’s UNIX socket
- Sends a request (`list`)
- Receives the neighbor table
- Prints neighbors grouped by device ID and for each device ID, displays all interfaces that 2 machines are connected via

Output info (per device):
- Device ID
- Interface name
- MAC address
- IPv4/IPv6 addresses
NOTE: Decided to not include last seen time because if every 30 seconds inactive machines are removed, this time does not really add any value

## Message protocol
### Ethernet frame
The daemon sends ethernet frames with the following payload
```
DESTINATION MAC    6 bytes
SOURCE MAC         6 bytes
ETH TYPE           2 bytes

MAGIC              4 bytes (MKTK)
DEVICE ID          32 bytes (not null terminated, taken from `/etc/machine-id`, this path is guaranteed to exist on the latest ubuntu)
IPv4               4 bytes (zeroed if does not exist)
IPv6               16 bytes (zeroed if does not exist)
```

### CLI message
For CLI message, daemon formats the text in a form which will be displayed in the terminal for the human and just sends plain text. CLI retrieves it and writes to the stdout without processing it

## Build and run
### Build
```bash
make daemon
make cli
```

### Run
#### Daemon (requires root privileges for the socket access)
```bash
sudo ./daemon
```

#### CLI
```bash
./cli list
```

Example output
```bash
DEVICE ID: 21be948a86594808ad6f072ce545840e
    enp7s0
        MAC: 52:54:00:14:12:65
        IPv4: 192.168.100.190
        IPv6: fe80::8ab3:d653:1488:b0a0
    enp1s0
        MAC: 52:54:00:af:40:4e
        IPv4: 192.168.122.93
        IPv6: fe80::5054:ff:feaf:404e
```

## Notes / Summary:
- If tested with cloned virtual machines, their id's in `/etc/machine-id` will be the same. That will never be the case with real life machines. If testing is done with cloned VM's, here is a snippet that will generate a new id:
```bash
sudo rm /etc/machine-id
sudo rm /var/lib/dbus/machine-id 2>/dev/null
sudo systemd-machine-id-setup
```
- The service uses raw ethernet sockets so root or `CAP_NET_RAW` privileges are required to run the daemon
- Only real, wired ethernet interfaces are included
- "hello" frame is sent every 5 seconds, neighbors are removed after 30 seconds of being inactive, sockets are closed if they have been idle for more than 15 seconds
