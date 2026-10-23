#include "utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void print_hex_dump(const unsigned char *buffer, size_t length) {
  printf("--- Hex Dump (%zu bytes) ---\n", length);

  for (size_t i = 0; i < length; i += 16) {
    printf("%04zx: ", i);

    for (size_t j = 0; j < 16; j++) {
      if (i + j < length) {
        printf("%02X ", buffer[i + j]);
      } else {
        printf("   ");
      }
    }

    printf(" | ");

    for (size_t j = 0; j < 16; j++) {
      if (i + j < length) {
        unsigned char c = buffer[i + j];
        printf("%c", isprint(c) ? c : '.');
      }
    }
    printf("\n");
  }

  printf("----------------------------\n");
}

/* Internet checksum (RFC 1071): the one's-complement of the one's-complement
 * sum of the data as 16-bit big-endian words.
 *
 * Returns the checksum in host byte order: store it with htons().
 * Run over data that already contains a correct checksum, it returns 0. */
uint16_t inet_checksum(const void *data, size_t len) {
  const uint8_t *p = data;
  uint32_t sum = 0;

  /* Build each word from two bytes rather than casting to uint16_t *:
   * no alignment or strict-aliasing problems, and no host byte order */
  while (len > 1) {
    sum += (uint32_t)p[0] << 8 | p[1];
    p += 2;
    len -= 2;
  }

  /* Odd length: the last byte is the high half of a zero-padded word */
  if (len == 1)
    sum += (uint32_t)p[0] << 8;

  /* End-around carry: fold everything above bit 15 back into the low 16 */
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return (uint16_t)~sum;
}
