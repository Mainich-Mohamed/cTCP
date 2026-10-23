#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

void print_hex_dump(const unsigned char *buffer, size_t length);
uint16_t inet_checksum(const void *data, size_t len);

#endif
