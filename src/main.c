#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include "tap.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  char dev_name[IFNAMSIZ] = "tap0";
  int tap_fd;

  printf("Attempting to create interface...\n");

  tap_fd = tap_alloc(dev_name);
  if (tap_fd < 0) {
    printf("Fatal: Could not create interface\n");
    exit(1);
  }

  printf("Successfully allocated TAP interface: %s\n", dev_name);
  printf("Listening for raw packets...\n");

  // Buffer that holds the raw bytes of an incomimg packet (standard Ethernet MTU is 1500)
  unsigned char buffer[1522];

  while (1) {
    // Freez until the kernel sends a packet to our TAP interface
    ssize_t nbytes = read(tap_fd, buffer, sizeof(buffer));

    if (nbytes < 0) {
      perror("Error reading from TAP");
      break;
    }

    printf("Caught a packet! Length: %zd bytes\n", nbytes);

    print_hex_dump(buffer, nbytes);
  }

  close(tap_fd);
  return 0;
}
