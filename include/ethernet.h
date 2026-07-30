#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <sys/types.h>

/* --- Sizes & Limits ----------------------------------- */
#define ETH_HDR_LEN       14  /* Fixed Ethernet II header */
#define ETH_MAC_LEN        6  /* 48-bit address           */
#define ETH_MAX_PAYLOAD 1500  /* Standard MTU             */
#define ETH_MIN_PAYLOAD   46  /* Added padding if needed  */

/* --- EtherType Constants ------------------------------------------------- */
/*
 * IEEE 802.3 rule: if the 2-byte field at offset 12 is <= 0x05DC (1500),
 * it is a LENGTH field (802.3 frame), NOT an EtherType (Ethernet II frame).
 * Only values >= 0x0600 are valid EtherTypes.
 */
#define ETH_TYPE_BOUNDARY 0x0600 
#define ETHERNET_IPV4 0x0800
#define ETHERNET_ARP  0x0806
#define ETHERNET_VLAN 0x8100
#define ETHERNET_IPV6 0x86DD

/* --- Ethernet II Header ----------- */
struct eth_hdr {
  uint8_t dst[ETH_MAC_LEN];  /* Destination MAC */
  uint8_t src[ETH_MAC_LEN];  /* Source MAC      */
  uint16_t etherType;        
} __attribute__((packed));

/* --- Public API -------------------- */
void eth_recv(const uint8_t *frame, ssize_t len);

#endif
