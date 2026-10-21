#include "arp.h"
#include "ethernet.h"
#include "netif.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static struct arp_entry cache[ARP_CACHE_SIZE];

/* Milliseconds from a monotonic clock: never jumps back like wall-clock time */
static uint64_t now_ms(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int arp_entry_expired(const struct arp_entry *e, uint64_t now) {
  return now - e->confirmed_ms > ARP_CACHE_TTL_MS;
}

/* Returns the index of the live entry for ip, or -1 if there is none.
 * An expired entry is freed on the spot (lazy expiration).
 * ip is in network byte order, like netif->ip. */
static int arp_cache_find(uint32_t ip) {
  uint64_t now = now_ms();

  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (!cache[i].valid || cache[i].ip != ip)
      continue;

    if (arp_entry_expired(&cache[i], now)) {
      cache[i].valid = 0;
      return -1;
    }
    return i;
  }
  return -1;
}

/* Slot for a new entry: a free or expired one if any, else the least recently
 * used */
static int arp_cache_victim(uint64_t now) {
  int lru = 0;

  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (!cache[i].valid || arp_entry_expired(&cache[i], now))
      return i;
    if (cache[i].used_ms < cache[lru].used_ms)
      lru = i;
  }
  return lru;
}

/* Adds or refreshes the ip -> mac mapping; both counts as a confirmation */
static void arp_cache_insert(uint32_t ip, const uint8_t *mac) {
  uint64_t now = now_ms();
  int i = arp_cache_find(ip);

  if (i < 0)
    i = arp_cache_victim(now);

  cache[i].ip = ip;
  memcpy(cache[i].mac, mac, ETH_MAC_LEN);
  cache[i].valid = 1;
  cache[i].confirmed_ms = now;
  cache[i].used_ms = now;
}

/* Copies the mac for ip into mac_out. Returns 0 if found, -1 if not. */
int arp_cache_lookup(uint32_t ip, uint8_t *mac_out) {
  int i = arp_cache_find(ip);

  if (i < 0)
    return -1;

  cache[i].used_ms = now_ms();
  memcpy(mac_out, cache[i].mac, ETH_MAC_LEN);
  return 0;
}

void arp_cache_dump(void) {
  char ip_str[INET_ADDRSTRLEN];
  uint64_t now = now_ms();

  printf("[ARP] Cache:\n");
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (!cache[i].valid)
      continue;

    inet_ntop(AF_INET, &cache[i].ip, ip_str, sizeof(ip_str));
    printf("  %-15s  %02X:%02X:%02X:%02X:%02X:%02X  age %llus\n", ip_str,
           cache[i].mac[0], cache[i].mac[1], cache[i].mac[2], cache[i].mac[3],
           cache[i].mac[4], cache[i].mac[5],
           (unsigned long long)((now - cache[i].confirmed_ms) / 1000));
  }
}

/* Formats a MAC as "AA:BB:CC:DD:EE:FF"; buf needs at least 18 bytes */
static const char *arp_mac_str(const uint8_t *mac, char *buf, size_t size) {
  snprintf(buf, size, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  return buf;
}

/* Answers a request for our IP with "netif->ip is-at netif->mac".
 * The requester's addresses become the target fields, and the reply goes
 * unicast to its MAC. Returns 0 if the frame was written, -1 if not. */
static int arp_send_reply(struct netif *netif, const struct arp_hdr *req) {
  struct arp_hdr reply;

  reply.htype = htons(ARP_HTYPE_ETH);
  reply.ptype = htons(ARP_PTYPE_IPV4);
  reply.hlen = ARP_HLEN_ETH;
  reply.plen = ARP_PLEN_IPV4;
  reply.oper = htons(ARP_OP_REPLY);

  /* Sender = us. netif->ip is already in network byte order: copy, no htonl */
  memcpy(reply.sha, netif->mac, ETH_MAC_LEN);
  memcpy(reply.spa, &netif->ip, sizeof(reply.spa));

  /* Target = whoever asked */
  memcpy(reply.tha, req->sha, ETH_MAC_LEN);
  memcpy(reply.tpa, req->spa, sizeof(reply.tpa));

  return eth_send(netif, req->sha, ETHERNET_ARP, (const uint8_t *)&reply,
                  sizeof(reply));
}

/* Entry point for an ARP packet handed up by eth_recv().
 *
 * payload points just past the Ethernet header. len can be larger than
 * ARP_HDR_LEN: Ethernet pads short frames up to its 46-byte minimum payload.
 */
void arp_recv(struct netif *netif, const uint8_t *payload, ssize_t len) {
  if (len < ARP_HDR_LEN) {
    printf("[ARP] Truncated packet: %zd bytes, minimum is %d - discarding\n",
           len, ARP_HDR_LEN);
    return;
  }

  /* Safe after the length check; packed struct, so no alignment padding */
  const struct arp_hdr *arp = (const struct arp_hdr *)payload;

  uint16_t htype = ntohs(arp->htype);
  uint16_t ptype = ntohs(arp->ptype);

  /* Only Ethernet/IPv4 ARP is supported: anything else would make the
   * fixed-size sha/spa/tha/tpa fields of struct arp_hdr meaningless */
  if (htype != ARP_HTYPE_ETH || ptype != ARP_PTYPE_IPV4 ||
      arp->hlen != ARP_HLEN_ETH || arp->plen != ARP_PLEN_IPV4) {
    printf("[ARP] Unsupported: htype=%u ptype=0x%04X hlen=%u plen=%u"
           " - discarding\n",
           htype, ptype, arp->hlen, arp->plen);
    return;
  }

  /* Kept in network byte order, like netif->ip; memcpy because spa/tpa
   * are byte arrays inside a packed struct, not aligned uint32_t */
  uint32_t spa, tpa;
  memcpy(&spa, arp->spa, sizeof(spa));
  memcpy(&tpa, arp->tpa, sizeof(tpa));

  char spa_str[INET_ADDRSTRLEN], tpa_str[INET_ADDRSTRLEN], sha_str[18];
  inet_ntop(AF_INET, &spa, spa_str, sizeof(spa_str));
  inet_ntop(AF_INET, &tpa, tpa_str, sizeof(tpa_str));
  arp_mac_str(arp->sha, sha_str, sizeof(sha_str));

  uint16_t oper = ntohs(arp->oper);

  switch (oper) {
  case ARP_OP_REQUEST:
    printf("[ARP] Request: who-has %s tell %s (%s)\n", tpa_str, spa_str,
           sha_str);
    break;

  case ARP_OP_REPLY:
    printf("[ARP] Reply: %s is-at %s\n", spa_str, sha_str);
    break;

  default:
    printf("[ARP] Unknown opcode %u - discarding\n", oper);
    return; /* Dropped before the cache update: stricter than RFC 826 */
  }

  /* --- Sender sanity checks ------------------------------------- */

  /* Bit 0 of the first byte marks a group (broadcast/multicast) address,
   * which no real NIC has. Cheap guard against cache poisoning */
  if (arp->sha[0] & 0x01) {
    printf("[ARP] Sender MAC %s is broadcast/multicast - discarding\n",
           sha_str);
    return;
  }

  /* Someone else claims our IP: address conflict. Never cache our own IP */
  if (spa == netif->ip) {
    printf("[ARP] Address conflict: %s claims our IP %s - discarding\n",
           sha_str, spa_str);
    return;
  }

  /* --- Cache update: RFC 826 receive algorithm ------------------ */

  int for_us = (tpa == netif->ip);

  /* An ARP probe (RFC 5227) has sender IP 0.0.0.0: nothing to learn */
  if (spa != 0) {
    int merge = arp_cache_find(spa) >= 0; /* Sender already known? */

    /* Known senders are refreshed by any packet; unknown senders are only
     * learned from packets aimed at us. insert() refreshes in place */
    if (merge || for_us) {
      arp_cache_insert(spa, arp->sha);
    }
  }

  if (!for_us)
    return;

  arp_cache_dump();

  /* RFC 826: only now look at the opcode */
  if (oper == ARP_OP_REQUEST) {
    char our_mac_str[18];

    if (arp_send_reply(netif, arp) == 0)
      printf("[ARP] Reply sent: %s is-at %s -> %s\n", tpa_str,
             arp_mac_str(netif->mac, our_mac_str, sizeof(our_mac_str)),
             sha_str);
  }
}
