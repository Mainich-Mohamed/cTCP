#ifndef IPV4_H
#define IPV4_H

#include "ethernet.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

/* --- IPv4 constants ---------------------------- */
#define IPV4_HDR_MIN_LEN 20 /* IHL = 5: no options */
#define IPV4_HDR_MAX_LEN 60 /* IHL = 15: the 4-bit field's maximum */
#define IPV4_VERSION 4

/* Masks for the 16-bit flags_frag field, after ntohs() */
#define IPV4_FLAG_DF 0x4000     /* Don't Fragment */
#define IPV4_FLAG_MF 0x2000     /* More Fragments */
#define IPV4_FRAG_OFFSET 0x1FFF /* Fragment offset, in 8-byte units */

/* Protocol field values (IANA "Assigned Internet Protocol Numbers") */
#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP 6
#define IPV4_PROTO_UDP 17

/* --- IPv4 Header (RFC 791 §3.1) ---------------------------- */
struct ipv4_hdr {
  uint8_t ver_ihl;     /* Version (high 4 bits), IHL in 32-bit words (low 4) */
  uint8_t tos;         /* Type of Service, now DSCP + ECN: treated as opaque */
  uint16_t total_len;  /* Header + data, in bytes */
  uint16_t id;         /* Identification, groups the fragments of a datagram */
  uint16_t flags_frag; /* Flags (high 3 bits), fragment offset (low 13) */
  uint8_t ttl;         /* Time to Live */
  uint8_t proto;       /* Upper-layer protocol (ICMP=1, TCP=6, UDP=17) */
  uint16_t checksum;   /* Header checksum (RFC 1071) */
  uint8_t saddr[4];    /* Source address */
  uint8_t daddr[4];    /* Destination address */
} __attribute__((packed));

struct netif;

void ipv4_recv(struct netif *netif, const uint8_t *payload, ssize_t len);

#endif
