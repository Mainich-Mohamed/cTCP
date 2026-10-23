#include "ipv4.h"
#include "netif.h"
#include "utils.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Entry point for an IPv4 packet handed up by eth_recv().
 *
 * payload points just past the Ethernet header. len can be larger than the
 * packet: Ethernet pads short frames up to its 46-byte minimum payload, so
 * the real length comes from the total_len field, not from len.
 */
void ipv4_recv(struct netif *netif, const uint8_t *payload, ssize_t len) {
  (void)netif; /* Used by the address checks in the next commit */

  if (len < IPV4_HDR_MIN_LEN) {
    printf("[IPv4] Truncated packet: %zd bytes, minimum is %d - discarding\n",
           len, IPV4_HDR_MIN_LEN);
    return;
  }

  /* Safe after the length check; packed struct, so no alignment padding */
  const struct ipv4_hdr *ip = (const struct ipv4_hdr *)payload;

  /* Version and IHL share one byte; IHL counts 32-bit words */
  uint8_t version = ip->ver_ihl >> 4;
  uint8_t hdr_len = (ip->ver_ihl & 0x0F) * 4;

  if (version != IPV4_VERSION) {
    printf("[IPv4] Bad version %u - discarding\n", version);
    return;
  }

  if (hdr_len < IPV4_HDR_MIN_LEN || hdr_len > len) {
    printf("[IPv4] Bad header length %u (packet %zd bytes) - discarding\n",
           hdr_len, len);
    return;
  }

  /* From here on total_len is the packet's real size: anything past it in
   * len is Ethernet padding */
  uint16_t total_len = ntohs(ip->total_len);
  if (total_len < hdr_len || total_len > len) {
    printf("[IPv4] Bad total length %u (header %u, received %zd) -"
           " discarding\n",
           total_len, hdr_len, len);
    return;
  }

  /* Summing a header that includes a correct checksum gives 0. Covers the
   * header and options only: the payload has its own checksum, if any */
  if (inet_checksum(ip, hdr_len) != 0) {
    printf("[IPv4] Bad header checksum 0x%04X - discarding\n",
           ntohs(ip->checksum));
    return;
  }

  /* Kept in network byte order, like netif->ip; memcpy because saddr/daddr
   * are byte arrays inside a packed struct, not aligned uint32_t */
  uint32_t saddr, daddr;
  memcpy(&saddr, ip->saddr, sizeof(saddr));
  memcpy(&daddr, ip->daddr, sizeof(daddr));

  char saddr_str[INET_ADDRSTRLEN], daddr_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &saddr, saddr_str, sizeof(saddr_str));
  inet_ntop(AF_INET, &daddr, daddr_str, sizeof(daddr_str));

  printf("[IPv4] %s -> %s proto %u ttl %u len %u (header %u)\n", saddr_str,
         daddr_str, ip->proto, ip->ttl, total_len, hdr_len);
}
