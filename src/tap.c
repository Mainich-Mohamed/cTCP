#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#include <stdint.h>
#include <stdio.h>
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
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
  }

  // Register the interface with the Linux kernel
  if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
    perror("Failed to register the interface with the Linux Kernel");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  dev[IFNAMSIZ - 1] = '\0';
  return fd;
}

// Retrieves the hardware MAC address of a named interface via ioctl
int tap_get_mac(const char *dev, uint8_t *mac_out) {
  struct ifreq ifr;
  int sock;

  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("tap_get_mac: socket");
    return -1;
  }

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
  ifr.ifr_name[IFNAMSIZ - 1] = '\0';
  if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
    perror("tap_get_mac: SIOCGIFHWADDR");
    close(sock);
    return -1;
  }

  memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);
  close(sock);
  return 0;
}
