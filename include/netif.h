#ifndef NETIF_H
#define NETIF_H

#include "ethernet.h"
#include <cstdint>
#include <stdint.h>

struct netif {
  int tap_fd;               /* TAP file descriptor */
  uint8_t mac[ETH_MAC_LEN]; /* Hardware (MAC) address */
  uint32_t ip;              /* IPv4 address */
  uint32_t netmask;         /* Subnet mask */
};

#endif // !NETIF_H
