/*
 * Copyright (C) 2022, 2023  Xiaoyue Chen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "network.h"
#include "sys/socket.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <unistd.h>

static int sock;
static struct sockaddr_in sock_addr_other;

void net_init(unsigned short port_self, const char *hostname_other,
              unsigned short port_other) {
  
  udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_sock < 0) {
    perror("socket");
    exit(1);
  }

  

  bind(udp_sock, (struct sockaddr *)&sock_addr_self, sizeof(sock_addr_self));
  if (bind(udp_sock, (struct sockaddr *)&sock_addr_self, sizeof(sock_addr_self)) < 0) {
    perror("bind");
    exit(1);
  }



                
  /*
   * TODO:
   *
   * 1. Create a UDP socket.
   *
   * 2. Bind the socket to port_self.
   *
   * 3. Set sock_addr_other to the socket address at hostname_other and
   * port_other.
   *
   */
}

void net_fini() { close(sock);/* TODO: Shutdown the socket. */ }

static void serialise(unsigned char *buff, const net_packet_t *pkt) {
  /* TODO:
   *
   * Serialise the packet according to the protocol.
   *
   * Note that it must use network endian.
   */
}

static void deserialise(net_packet_t *pkt, const unsigned char *buff) {
  /* TODO: Deserialise the packet into the net_packet structure. */
}

int net_poll(net_packet_t *pkt) {
  /* TODO: Poll a packet from the socket.
   *
   * Returns 0 if nothing to be read from the socket.
   *
   * Returns 1 otherwise.
   */
  return 0;
}

void net_send(const net_packet_t *pkt) {
  /* TODO: Serialise and send the packet to the other's socket. */
}
