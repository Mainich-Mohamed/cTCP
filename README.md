# cTCP

A TCP/IP stack written from scratch in C, running entirely in userspace.

https://github.com/user-attachments/assets/7d8e7c74-f925-4d2b-bbe4-0f044b506145


cTCP opens a Linux TAP interface, reads raw Ethernet frames straight off the wire, parses every protocol layer by hand, and builds its replies byte by byte. It uses no kernel networking stack and no third-party libraries: just the RFCs, `read()`, and `write()`.

On the `tap0` "cable", cTCP is its own host: **10.0.0.2**, MAC **02:00:00:00:00:01** (a locally administered address). The host kernel's side of `tap0` is 10.0.0.1.

The goal is to climb the stack one layer at a time until a real TCP connection works.

## Status

| Layer | Spec | Status |
|---|---|---|
| TAP device I/O | Linux TUN/TAP | ✅ Done |
| Ethernet II | IEEE 802.3 | ✅ Done |
| ARP | [RFC 826](https://datatracker.ietf.org/doc/html/rfc826) | ✅ Done (replies to requests; doesn't send its own yet) |
| IPv4 | [RFC 791](https://datatracker.ietf.org/doc/html/rfc791) | 🚧 Next |
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
                              │
                     arp_recv()  validate, update cache, reply
                              │
                     eth_send()  build frame, pad to 60 bytes
                              │
  host kernel ◄─── tap0 ◄──── write()
```

1. `tap_alloc()` opens `/dev/net/tun` and registers a TAP interface (`IFF_TAP | IFF_NO_PI`), so the program receives raw Ethernet frames with no extra packet-info header.
2. The main loop blocks on `read()` until the kernel hands over a frame.
3. `eth_recv()` validates the frame length, decodes the destination and source MACs, converts the EtherType from network byte order, and dispatches the payload to the next layer.
4. Frames whose type field is ≤ 1500 are recognized as 802.3 length fields rather than EtherTypes, and are not dispatched.
5. `arp_recv()` accepts only Ethernet/IPv4 ARP. It drops ARP probes (sender 0.0.0.0) from the cache, along with multicast sender MACs and packets that claim our own IP.
6. The cache follows the RFC 826 merge rule: known senders are refreshed by any packet, unknown senders are only learned from packets aimed at us. It holds 32 entries with LRU eviction and a 60 s expiry.
7. A request for 10.0.0.2 gets a unicast reply. `eth_send()` adds the Ethernet header, zero-pads the frame to the 60-byte minimum, and writes it to the TAP fd in one `write()`.

Example output for an incoming ARP request:

```
Stack address: 10.0.0.2 / 255.255.255.0 / 02:00:00:00:00:01
Listening for raw packets...

--- Packet (42 bytes) ---
--- Hex Dump (42 bytes) ---
0000: FF FF FF FF FF FF 86 7E 7B 87 F4 77 08 06 00 01  | .......~{..w....
0010: 08 00 06 04 00 01 86 7E 7B 87 F4 77 0A 00 00 01  | .......~{..w....
0020: 00 00 00 00 00 00 0A 00 00 02                    | ..........
----------------------------
[ETH] Dst: FF:FF:FF:FF:FF:FF
[ETH] Src: 86:7E:7B:87:F4:77
[ETH] EtherType: 0x0806
-> ARP
[ARP] Request: who-has 10.0.0.2 tell 10.0.0.1 (86:7E:7B:87:F4:77)
[ARP] Cache:
  10.0.0.1         86:7E:7B:87:F4:77  age 0s
[ARP] Reply sent: 10.0.0.2 is-at 02:00:00:00:00:01 -> 86:7E:7B:87:F4:77
```

The host-side MAC of `tap0` is random and changes every time the interface is created.

## Project layout

```
include/
  ethernet.h   Ethernet II header, EtherType constants
  arp.h        ARP header, cache entry, public API
  netif.h      Network interface state (TAP fd, MAC, IP, netmask)
  tap.h        TAP allocation and MAC lookup
  utils.h      Hex dump helper
src/
  main.c       Stack identity (IP, netmask, MAC), interface setup, read loop
  tap.c        tap_alloc(), tap_get_mac() (SIOCGIFHWADDR ioctl)
  ethernet.c   Frame parsing, protocol dispatch, eth_send()
  arp.c        ARP parsing, cache (LRU + expiry), replies
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

cTCP answers the host's ARP request, so the host learns its MAC:

```sh
ip neigh show dev tap0   # 10.0.0.2 lladdr 02:00:00:00:00:01 REACHABLE
```

The ping itself still times out: answering ICMP echo is the next milestone.

## References

- [RFC 826: An Ethernet Address Resolution Protocol](https://datatracker.ietf.org/doc/html/rfc826)
- [RFC 791: Internet Protocol](https://datatracker.ietf.org/doc/html/rfc791)
- [RFC 792: Internet Control Message Protocol](https://datatracker.ietf.org/doc/html/rfc792)
- [RFC 768: User Datagram Protocol](https://datatracker.ietf.org/doc/html/rfc768)
- [RFC 9293: Transmission Control Protocol](https://datatracker.ietf.org/doc/html/rfc9293)
- [Linux TUN/TAP documentation](https://www.kernel.org/doc/Documentation/networking/tuntap.txt)
