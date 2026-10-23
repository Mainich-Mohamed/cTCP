#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"
#include "netif.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void eth_print_mac(const uint8_t *mac) {
  printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
         mac[4], mac[5]);
}

/* Entry point for all Ethernet frame processing.
 *
 * Memory layout when this is called:
 *   frame[0..5]   → Destination MAC
 *   frame[6..11]  → Source MAC
 *   frame[12..13] → EtherType (big-endian)
 *   frame[14..]   → Payload (handed off to next layer)
 */
void eth_recv(struct netif *netif, const uint8_t *frame, ssize_t len) {
  if (len < ETH_HDR_LEN) {
    printf("[ETH] Runt frame: %zd bytes, minimum is %d - discarding\n", len,
           ETH_HDR_LEN);
    return;
  }

  const struct eth_hdr *eth = (const struct eth_hdr *)frame;

  /* The wire sends this field big-endian. x64 is little-endian */
  uint16_t etherType = ntohs(eth->etherType);

  printf("[ETH] Dst: ");
  eth_print_mac(eth->dst);
  printf("\n[ETH] Src: ");
  eth_print_mac(eth->src);
  printf("\n[ETH] EtherType: 0x%04X\n", etherType);

  if (etherType < ETH_TYPE_BOUNDARY) {
    printf("-> 802.3 Length Field (%u bytes payload) - no dispatch\n",
           etherType);
    return;
  }

  const uint8_t *payload = frame + ETH_HDR_LEN;
  ssize_t payload_len = len - ETH_HDR_LEN;

  switch (etherType) {
  case ETHERNET_ARP:
    printf("-> ARP\n");
    arp_recv(netif, payload, payload_len);
    break;

  case ETHERNET_IPV4:
    printf("-> IPv4\n");
    ipv4_recv(netif, payload, payload_len);
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

/* Wraps payload in an Ethernet II header and writes it to the TAP device.
 *
 * Frame built here:
 *   frame[0..5]   → dst_mac
 *   frame[6..11]  → netif->mac (we are always the source)
 *   frame[12..13] → ethertype (big-endian)
 *   frame[14..]   → payload, zero-padded to ETH_MIN_PAYLOAD
 *
 * No FCS (CRC) is added: the TAP device works on frames without it.
 * Returns 0 on success, -1 on error.
 */
int eth_send(struct netif *netif, const uint8_t *dst_mac, uint16_t ethertype,
             const uint8_t *payload, size_t len) {
  if (len > ETH_MAX_PAYLOAD) {
    printf("[ETH] Payload too large: %zu bytes, maximum is %d - not sent\n",
           len, ETH_MAX_PAYLOAD);
    return -1;
  }

  uint8_t frame[ETH_HDR_LEN + ETH_MAX_PAYLOAD];
  struct eth_hdr *eth = (struct eth_hdr *)frame;

  memcpy(eth->dst, dst_mac, ETH_MAC_LEN);
  memcpy(eth->src, netif->mac, ETH_MAC_LEN);
  eth->etherType = htons(ethertype); /* x64 is little-endian, the wire is not */
  memcpy(frame + ETH_HDR_LEN, payload, len);

  /* A real NIC pads short frames in hardware. On TAP we hand the kernel the
   * finished frame, so we pad to the 46-byte minimum ourselves */
  size_t payload_len = len;
  if (payload_len < ETH_MIN_PAYLOAD) {
    memset(frame + ETH_HDR_LEN + len, 0, ETH_MIN_PAYLOAD - len);
    payload_len = ETH_MIN_PAYLOAD;
  }

  /* On TAP, one write() is exactly one frame */
  size_t frame_len = ETH_HDR_LEN + payload_len;
  ssize_t n = write(netif->tap_fd, frame, frame_len);
  if (n < 0) {
    perror("[ETH] write to TAP");
    return -1;
  }
  if ((size_t)n != frame_len) {
    printf("[ETH] Short write: %zd of %zu bytes\n", n, frame_len);
    return -1;
  }
  return 0;
}
