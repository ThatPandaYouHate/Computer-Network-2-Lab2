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
#include <stdio.h>
#include <unistd.h>

static int sock;
static struct sockaddr_in sock_addr_other;

void net_init(unsigned short port_self, const char *hostname_other,
              unsigned short port_other) {

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

  /* 1. Skapa UDP-socket */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    const char msg[] = "socket failed\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
  }
  printf("socket created: %d", sock);
  

  /* 2. Binda socketen till port_self på alla interface */
  struct sockaddr_in sock_addr_self = {0};
  sock_addr_self.sin_family = AF_INET;
  sock_addr_self.sin_addr.s_addr = INADDR_ANY;
  sock_addr_self.sin_port = htons(port_self);

  if (bind(sock, (struct sockaddr *)&sock_addr_self, sizeof(sock_addr_self)) <
      0) {
    const char msg[] = "bind failed\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
  }

  /* 3. Sätt mottagarens adress (hostname_other, port_other) */
  sock_addr_other = (struct sockaddr_in){0};
  sock_addr_other.sin_family = AF_INET;
  sock_addr_other.sin_port = htons(port_other);

  /* Försök tolka hostname som IP-sträng först, annars via DNS-lookup */
  if (inet_aton(hostname_other, &sock_addr_other.sin_addr) == 0) {
    struct hostent *host = gethostbyname(hostname_other);
    if (host == NULL || host->h_addr_list[0] == NULL) {
      const char msg[] = "resolve failed\n";
      write(STDERR_FILENO, msg, sizeof(msg) - 1);
      _exit(1);
    }
    sock_addr_other.sin_addr = *(struct in_addr *)host->h_addr_list[0];
  }
}

void net_fini() { close(sock);/* TODO: Shutdown the socket. */ }

static void serialise(unsigned char *buff, const net_packet_t *pkt) {
  /* TODO:
   *
   * Serialise the packet according to the protocol.
   *
   * Note that it must use network endian.
   */

  buff[0] = pkt->opcode;
  buff[1] = pkt->epoch >> 8;
  buff[2] = pkt->epoch & 0xFF;
  buff[3] = pkt->input;
}

static void deserialise(net_packet_t *pkt, const unsigned char *buff) {
  /* TODO: Deserialise the packet into the net_packet structure. */
  pkt->opcode = buff[0];
  pkt->epoch = (buff[1] << 8) | buff[2];
  pkt->input = buff[3];
}

int net_poll(net_packet_t *pkt) {
  /* TODO: Poll a packet from the socket.
   *
   * Returns 0 if nothing to be read from the socket.
   *
   * Returns 1 otherwise.
   */
  // Use select to poll for available data, non-blocking.
  fd_set rfds;
  struct timeval tv;
  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0; // Non-blocking

  int retval = select(sock + 1, &rfds, NULL, NULL, &tv);
  if (retval <= 0) {
    // Nothing to read or error (treat error as nothing).
    return 0;
  }

  // There is data to read; read 4 bytes into buffer.
  unsigned char buff[4];
  int bytes_read = read(sock, buff, sizeof(buff));
  if (bytes_read != 4) {
    // Not a valid full packet, treat as no packet.
    return 0;
  }
  deserialise(pkt, buff);
  return 1;
}

void net_send(const net_packet_t *pkt) {
  /* TODO: Serialise and send the packet to the other's socket. */

  unsigned char buff[4];
  serialise(buff, pkt);
  sendto(sock, buff, sizeof(buff), 0, (struct sockaddr *)&sock_addr_other, sizeof(sock_addr_other));
}
