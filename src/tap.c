#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

// Function to allocate the TAP interface
int tap_alloc(char *dev) {
  struct ifreq ifr;
  int fd, err;

  // Open the TUN/TAP clone device
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    perror("Failed to open /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = IFF_TAP | IFF_NO_PI; 

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  // Register the interface with the Linux kernel
  if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
    perror("Failed to register the interface with the Linux Kernel");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  return fd;
}
