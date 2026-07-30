#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include "ethernet.h"

static void eth_print_mac(const uint8_t *mac) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X", 
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

 /* Entry point for all Ethernet frame processing.
 *
 * Memory layout when this is called:
 *   frame[0..5]   → Destination MAC
 *   frame[6..11]  → Source MAC
 *   frame[12..13] → EtherType (big-endian)
 *   frame[14..]   → Payload (handed off to next layer)
 */
void eth_recv(const uint8_t *frame, ssize_t len) {
  if (len < ETH_HDR_LEN) {
    printf("[ETH] Runt frame: %zd bytes, minimum is %d - discarding\n",
           len, ETH_HDR_LEN);
    return;
  }

  const struct eth_hdr *eth = (const struct eth_hdr *)frame;
  
  /* The wire sends this field big-endian. x64 is little-endian */
  uint16_t  etherType = ntohs(eth->etherType);
  
  printf("[ETH] Dst: ");
  eth_print_mac(eth->dst);
  printf("\n[ETH] Src: ");
  eth_print_mac(eth->src);
  printf("\n[ETH] EtherType: 0x%04X\n", etherType);

  if (etherType < ETH_TYPE_BOUNDARY) {
    printf("-> 802.3 Length Field (%u bytes payload) - no dispatch\n", etherType);
    return;
  }

  const uint8_t *payload = frame + ETH_HDR_LEN;
  ssize_t payload_len = len - ETH_HDR_LEN;

  switch (etherType) {
    case ETHERNET_ARP:
      printf("-> ARP\n");
    break;

    case ETHERNET_IPV4:
      printf("-> IPv4\n");
    break;

    case ETHERNET_IPV6:
      printf("-> IPv6\n");
    break;

    case ETHERNET_VLAN:
      printf("-> 802.1Q VLAN Tag (offset shift required)\n");
    break;

    default:
      printf("-> Unknown EtherType 0x%04X - discardig\n", etherType);
    break;
  }
}
