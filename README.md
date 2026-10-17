# cTCP

A TCP/IP stack written from scratch in C, running entirely in userspace.

https://github.com/user-attachments/assets/7d8e7c74-f925-4d2b-bbe4-0f044b506145


cTCP opens a Linux TAP interface, reads raw Ethernet frames straight off the wire, and parses every protocol layer by hand. It uses no kernel networking stack and no third-party libraries: just the RFCs, `read()`, and `write()`.

The goal is to climb the stack one layer at a time until a real TCP connection works.

## Status

| Layer | Spec | Status |
|---|---|---|
| TAP device I/O | Linux TUN/TAP | ✅ Done |
| Ethernet II | IEEE 802.3 | ✅ Done |
| ARP | [RFC 826](https://datatracker.ietf.org/doc/html/rfc826) | 🚧 In progress |
| IPv4 | [RFC 791](https://datatracker.ietf.org/doc/html/rfc791) | ⬜ Planned |
| ICMP (echo) | [RFC 792](https://datatracker.ietf.org/doc/html/rfc792) | ⬜ Planned |
| UDP | [RFC 768](https://datatracker.ietf.org/doc/html/rfc768) | ⬜ Planned |
| TCP | [RFC 9293](https://datatracker.ietf.org/doc/html/rfc9293) | ⬜ Planned |

**Next milestone:** answer a `ping` from the host.

## How it works

```
  host kernel ──── tap0 ────► /dev/net/tun ──► cTCP (userspace)
                                                  │
                                         eth_recv()  Ethernet II
                                                  │  dispatch on EtherType
                              ┌───────────────────┼───────────────────┐
                           0x0806              0x0800              0x86DD
                             ARP                IPv4                IPv6
```

1. `tap_alloc()` opens `/dev/net/tun` and registers a TAP interface (`IFF_TAP | IFF_NO_PI`), so the program receives raw Ethernet frames with no extra packet-info header.
2. The main loop blocks on `read()` until the kernel hands over a frame.
3. `eth_recv()` validates the frame length, decodes the destination and source MACs, converts the EtherType from network byte order, and dispatches the payload to the next layer.
4. Frames whose type field is ≤ 1500 are recognized as 802.3 length fields rather than EtherTypes, and are not dispatched.

Example output for an incoming ARP request:

```
--- Packet (42 bytes) ---
--- Hex Dump (42 bytes) ---
0000: FF FF FF FF FF FF 7A 3C 91 E4 05 B2 08 06 00 01  | ......z<........
0010: 08 00 06 04 00 01 7A 3C 91 E4 05 B2 0A 00 00 01  | ......z<........
0020: 00 00 00 00 00 00 0A 00 00 02                    | ..........
----------------------------
[ETH] Dst: FF:FF:FF:FF:FF:FF
[ETH] Src: 7A:3C:91:E4:05:B2
[ETH] EtherType: 0x0806
-> ARP
```

## Project layout

```
include/
  ethernet.h   Ethernet II header, EtherType constants
  arp.h        ARP header, cache entry, public API
  netif.h      Network interface state (TAP fd, MAC, IP, netmask)
  tap.h        TAP allocation and MAC lookup
  utils.h      Hex dump helper
src/
  main.c       Interface setup and packet read loop
  tap.c        tap_alloc(), tap_get_mac() (SIOCGIFHWADDR ioctl)
  ethernet.c   Frame parsing and protocol dispatch
  arp.c        ARP handling and cache (in progress)
  utils.c      print_hex_dump()
```

## Requirements

- Linux with TUN/TAP support (`/dev/net/tun`)
- A C compiler (GCC or Clang)
- CMake ≥ 3.10
- Root, or `CAP_NET_ADMIN`, to create the TAP interface

On Windows, use WSL2.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
sudo ./build/cTCP
```

`tap0` is created when cTCP starts. In a second terminal, bring it up and give the host side an address:

```sh
sudo ip link set tap0 up
sudo ip addr add 10.0.0.1/24 dev tap0
```

Then generate some traffic to watch cTCP parse it:

```sh
ping 10.0.0.2          # host sends ARP requests for 10.0.0.2 onto tap0
```

Until ARP replies are implemented, the ping won't get answers, but every frame will be dumped and decoded.

## Roadmap

- [ ] ARP: parse requests and replies, maintain the cache, reply for our own IP
- [ ] Wire ARP into the Ethernet dispatcher and give `main.c` a configured `netif`
- [ ] IPv4: header parsing, checksum validation, protocol dispatch
- [ ] ICMP echo reply, so `ping` works
- [ ] UDP
- [ ] TCP: state machine, sequence numbers, retransmission
- [ ] A minimal socket-like API on top

## References

- [RFC 826: An Ethernet Address Resolution Protocol](https://datatracker.ietf.org/doc/html/rfc826)
- [RFC 791: Internet Protocol](https://datatracker.ietf.org/doc/html/rfc791)
- [RFC 792: Internet Control Message Protocol](https://datatracker.ietf.org/doc/html/rfc792)
- [RFC 768: User Datagram Protocol](https://datatracker.ietf.org/doc/html/rfc768)
- [RFC 9293: Transmission Control Protocol](https://datatracker.ietf.org/doc/html/rfc9293)
- [Linux TUN/TAP documentation](https://www.kernel.org/doc/Documentation/networking/tuntap.txt)
