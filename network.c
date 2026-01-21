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

/* Network module state */
static int sock;                          /* UDP socket file descriptor */
static struct sockaddr_in sock_addr_other; /* Peer's socket address for sending packets */

/**
 * Initialize network module
 * Creates a UDP socket, binds it to the specified port, and resolves the peer's address.
 * 
 * @param port_self Local port to bind the socket to
 * @param hostname_other Peer's hostname or IP address
 * @param port_other Peer's port number
 */
void net_init(unsigned short port_self, const char *hostname_other,
              unsigned short port_other) {

  /* Step 1: Create UDP socket
   * AF_INET: IPv4 address family
   * SOCK_DGRAM: UDP socket type (datagram)
   * 0: Protocol (0 means default for socket type) */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    const char msg[] = "socket failed\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
  }
  printf("socket created: %d\n", sock);
  

  /* Step 2: Bind socket to local port
   * INADDR_ANY: Listen on all available network interfaces
   * htons(): Convert port number from host byte order to network byte order */
  struct sockaddr_in sock_addr_self = {0};
  sock_addr_self.sin_family = AF_INET;
  sock_addr_self.sin_addr.s_addr = INADDR_ANY;
  sock_addr_self.sin_port = htons(port_self);

  if (bind(sock, (struct sockaddr *)&sock_addr_self, sizeof(sock_addr_self)) < 0) {
    const char msg[] = "bind failed\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);
  }

  /* Step 3: Resolve peer's address
   * Initialize peer address structure */
  sock_addr_other = (struct sockaddr_in){0};
  sock_addr_other.sin_family = AF_INET;
  sock_addr_other.sin_port = htons(port_other);

  /* Try to parse hostname as IP address first (inet_aton)
   * If that fails, perform DNS lookup (gethostbyname) */
  if (inet_aton(hostname_other, &sock_addr_other.sin_addr) == 0) {
    struct hostent *host = gethostbyname(hostname_other);
    if (host == NULL || host->h_addr_list[0] == NULL) {
      const char msg[] = "resolve failed\n";
      write(STDERR_FILENO, msg, sizeof(msg) - 1);
      _exit(1);
    }
    /* Copy first IP address from DNS lookup result */
    sock_addr_other.sin_addr = *(struct in_addr *)host->h_addr_list[0];
  }
}

/**
 * Finalize network module
 * Closes the UDP socket and releases network resources.
 */
void net_fini() {
  close(sock);
}

/**
 * Serialize packet into byte buffer for network transmission
 * Packet format (4 bytes total):
 *   [0]: opcode (1 byte)
 *   [1-2]: epoch number (2 bytes, big-endian/network byte order)
 *   [3]: input command (1 byte)
 * 
 * @param buff Output buffer (must be at least 4 bytes)
 * @param pkt Packet structure to serialize
 */
static void serialise(unsigned char *buff, const net_packet_t *pkt) {
  buff[0] = pkt->opcode;
  /* Epoch is 16-bit: store high byte first (big-endian/network byte order) */
  buff[1] = pkt->epoch >> 8;      /* High byte */
  buff[2] = pkt->epoch & 0xFF;   /* Low byte */
  buff[3] = pkt->input;
}

/**
 * Deserialize byte buffer into packet structure
 * Reconstructs packet from network byte order format.
 * 
 * @param pkt Output packet structure
 * @param buff Input buffer containing serialized packet (4 bytes)
 */
static void deserialise(net_packet_t *pkt, const unsigned char *buff) {
  pkt->opcode = buff[0];
  /* Reconstruct 16-bit epoch from big-endian format */
  pkt->epoch = (buff[1] << 8) | buff[2];  /* High byte << 8 | Low byte */
  pkt->input = buff[3];
}

/**
 * Poll for incoming packet (non-blocking)
 * Checks if data is available on the socket and reads a complete packet if present.
 * 
 * @param pkt Output parameter: packet structure to fill if packet is received
 * @return 1 if a complete packet was received and deserialized
 * @return 0 if no packet is available or packet is incomplete
 */
int net_poll(net_packet_t *pkt) {
  /* Use select() for non-blocking I/O
   * Set timeout to 0 to make it non-blocking */
  fd_set rfds;
  struct timeval tv;
  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);
  tv.tv_sec = 0;
  tv.tv_usec = 0;  /* Zero timeout = non-blocking */

  /* Check if socket has data ready to read */
  int retval = select(sock + 1, &rfds, NULL, NULL, &tv);
  if (retval <= 0) {
    /* No data available or error occurred (treat error as no data) */
    return 0;
  }

  /* Data is available: read exactly 4 bytes (packet size) */
  unsigned char buff[4];
  int bytes_read = read(sock, buff, sizeof(buff));
  if (bytes_read != 4) {
    /* Incomplete packet: discard and return 0
     * This handles cases where packet is fragmented or corrupted */
    return 0;
  }
  
  /* Deserialize the received bytes into packet structure */
  deserialise(pkt, buff);
  return 1;
}

/**
 * Send packet to peer
 * Serializes the packet and sends it via UDP to the peer's address.
 * 
 * @param pkt Packet structure to send
 */
void net_send(const net_packet_t *pkt) {
  /* Serialize packet into byte buffer */
  unsigned char buff[4];
  serialise(buff, pkt);
  
  /* Send via UDP to peer's address (set during net_init)
   * sendto() is used for UDP (connectionless) communication */
  sendto(sock, buff, sizeof(buff), 0, 
         (struct sockaddr *)&sock_addr_other, sizeof(sock_addr_other));
}
