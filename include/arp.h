#ifndef ARP_H
#define ARP_H

#include "ethernet.h"
#include <stdint.h>
#include <sys/types.h>

/* --- ARP constants ---------------------------- */
#define ARP_HDR_LEN 28        /* Fixed for Ethernet/IPv4 ARP */
#define ARP_HTYPE_ETH 1       /* Hardware type: Ethernet */
#define ARP_PTYPE_IPV4 0x0800 /* Protocol type: IPv4 */
#define ARP_HLEN_ETH 6        /* Ethernet Hardware address length */
#define ARP_PLEN_IPV4 4       /* IPv4 protocol address length */
#define ARP_OP_REQUEST 1      /* Opcode: REQUEST */
#define ARP_OP_REPLY 2        /* Opcode: REPLY */

#define ARP_CACHE_SIZE 32

/* --- ARP Header ---------------------------- */
struct arp_hdr {
  uint16_t htype;
  uint16_t ptype;
  uint8_t hlen;
  uint8_t plen;
  uint16_t oper;            /* Operation (REQUEST=1, REPLY=2) */
  uint8_t sha[ETH_MAC_LEN]; /* Sender Hardware (MAC) address */
  uint8_t spa[4];           /* Sender protocol (IP) address */
  uint8_t tha[ETH_MAC_LEN]; /* Target Hardware (MAC) address */
  uint8_t tpa[4];           /* Target protocol (IP) address */
} __attribute__((packed));

/* --- ARP Cache ---------------------------- */
struct arp_entry {
  uint32_t ip;
  uint8_t mac[ETH_MAC_LEN];
  uint8_t valid; /* 1 == slot occupied, 0 == free */
};

struct netif;

void arp_recv(struct netif *netif, const uint8_t *payload, ssize_t len);
int arp_cache_lookup(uint32_t ip, uint8_t *mac_out);
void arp_cache_dump(void);

#endif
