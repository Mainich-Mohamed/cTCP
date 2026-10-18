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
