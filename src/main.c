#include "ethernet.h"
#include "netif.h"
#include "tap.h"
#include "utils.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* --- Stack identity -------------------------------------------
 * The stack is a separate host on the tap0 "cable": the kernel side
 * of tap0 is 10.0.0.1, the stack is 10.0.0.2.
 * 0x02 in the first MAC byte = locally administered address, so it
 * can never collide with a real NIC's vendor-assigned MAC.
 */

#define STACK_IP "10.0.0.2"
#define STACK_NETMASK "255.255.255.0"
// // Software stack lacks a hardware MAC (Burned-In Address), so we assign a
// locally-administered one
static const uint8_t STACK_MAC[ETH_MAC_LEN] = {0x02, 0x00, 0x00,
                                               0x00, 0x00, 0x01};

int main(int argc, char *argv[]) {
  char dev_name[IFNAMSIZ] = "tap0";
  struct netif netif;
  char ip_str[INET_ADDRSTRLEN];
  char mask_str[INET_ADDRSTRLEN];

  printf("Attempting to create interface...\n");

  netif.tap_fd = tap_alloc(dev_name);
  if (netif.tap_fd < 0) {
    printf("Fatal: Could not create interface\n");
    exit(1);
  }

  printf("Successfully allocated TAP interface: %s\n", dev_name);

  memcpy(netif.mac, STACK_MAC, ETH_MAC_LEN);

  /* inet_pton writes the address in network byte order: ip/netmask stay
   *  big-endian everywhere in the stack, so they can be compared to packet
   * fields with memcmp and no ntohl. Returns 1 on success. */
  if (inet_pton(AF_INET, STACK_IP, &netif.ip) != 1
     || inet_pton(AF_INET, STACK_NETMASK, &netif.netmask) != 1) {
      printf("Fatal: Invalid stack IP or netmask\n");
      close(netif.tap_fd);
      exit(1);
    }

  // Print the stored values back as text so they can be eyeballed at startup
  inet_ntop(AF_INET, &netif.ip, ip_str, sizeof(ip_str));
  inet_ntop(AF_INET, &netif.netmask, mask_str, sizeof(mask_str));
  printf("Stack address: %s / %s / %02X:%02X:%02X:%02X:%02X:%02X\n", 
         ip_str, mask_str, netif.mac[0], netif.mac[1], netif.mac[2], 
         netif.mac[3], netif.mac[4], netif.mac[5]);

  printf("Listening for raw packets...\n");

  // Buffer that holds the raw bytes of an incomimg packet (standard Ethernet
  // MTU is 1500)
  uint8_t buffer[1522];

  while (1) {
    // Freez until the kernel sends a packet to our TAP interface
    ssize_t nbytes = read(netif.tap_fd, buffer, sizeof(buffer));

    if (nbytes < 0) {
      perror("Error reading from TAP");
      break;
    }

    printf("\n--- Packet (%zd bytes) ---\n", nbytes);
    print_hex_dump(buffer, nbytes);
    eth_recv(&netif, buffer, nbytes);
  }

  close(netif.tap_fd);
  return 0;
}
