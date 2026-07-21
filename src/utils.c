#include <stdio.h>
#include <ctype.h>

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
