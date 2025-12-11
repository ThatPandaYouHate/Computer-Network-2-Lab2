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

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

typedef struct net_packet {
  /* TODO: Declare variables according to the protocol. */
  uint8_t opcode;
  uint16_t epoch;
  uint8_t input;
} net_packet_t;

void net_init(unsigned short port_self, const char *hostname_other,
              unsigned short port_other);
void net_fini();
void net_send(const net_packet_t *pkt);
int net_poll(net_packet_t *pkt);

#endif
