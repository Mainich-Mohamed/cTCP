#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "arp.h"
#include "netif.h"
#include "ethernet.h"

static struct arp_entry cache[ARP_CACHE_SIZE];
