#ifndef TAP_H
#define TAP_H

#include <stdint.h>

int tap_alloc(char *dev);
int tap_get_mac(const char *dev, uint8_t *mac_out);

#endif
