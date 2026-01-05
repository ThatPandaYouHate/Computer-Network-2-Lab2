/*
 * Copyright (C) 2022, 2023, 2024  Xiaoyue Chen
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
#include "simulate.h"
#include "unistd.h"
#include "window.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

static const int SCREEN_WIDTH = 720;
static const int SCREEN_HEIGHT = 640;
static const int SIM_INTERVAL = 10;


#define OPCODE_CMD 0
#define OPCODE_ACK 1

typedef struct epoch {
  bool cmd;
  bool ack;
  bool cmd_self;
} epoch_t;

static void usage(const char *program_name) {
  fprintf(stderr, "Usage: %s <self_port> <peer_hostname> <peer_port> <player>\n", program_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Arguments:\n");
  fprintf(stderr, "  self_port      Port to listen on (e.g. 9930)\n");
  fprintf(stderr, "  peer_hostname  Peer's hostname or IP address (e.g. 127.0.0.1)\n");
  fprintf(stderr, "  peer_port      Peer's port (e.g. 9931)\n");
  fprintf(stderr, "  player         Player number, 0 or 1\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Examples:\n");
  fprintf(stderr, "  %s 9930 127.0.0.1 9931 0\n", program_name);
  fprintf(stderr, "  %s 9931 127.0.0.1 9930 1\n", program_name);
}

int main(int argc, char *argv[argc + 1]) {
  if (argc != 5) {
    usage(argv[0]);
    return 1;
  }
  unsigned short port_self = atoi(argv[1]);  /* 9930 */
  const char *hostname_other = argv[2];      /* "127.0.0.1" */
  unsigned short port_other = atoi(argv[3]); /* 9931 */
  int player = atol(argv[4]);                /* 0 */
  int other_player = player == 0 ? 1 : 0;

  
  
  state_t state = sim_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  win_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  net_init(port_self, hostname_other, port_other);

  uint16_t epoch = 0;
  epoch_t epoch_state = {false, false, false};
  cmd_t cmds[2];
  bool quit = false;

  uint32_t previous_tick = win_tick();
  uint32_t epoch_start_tick = previous_tick;

  printf("game started\n");
  printf("waiting for player %d to start the game\n", other_player);
  while (!quit) {
    net_packet_t pkt;
    win_event_t e = win_poll_event();
    if (e.quit)
      quit = true;

    for (; win_tick() - previous_tick > SIM_INTERVAL;
        previous_tick += SIM_INTERVAL) {
      /*
       * TODO: Poll and handle each packet until no more packet.
       *
       * If we receive a command packet, send an acknowledgement packet, mark
       * its flag in epoch_state, and set the command in cmds array. If we
       * receive a acknowledge packet, just mark its flag in epoch_state.
       */
      
      while ((!epoch_state.cmd || !epoch_state.ack) && net_poll(&pkt)) {
        if (pkt.epoch == epoch) {
          switch (pkt.opcode) {
            case OPCODE_CMD:
              epoch_state.cmd = true;
              cmds[other_player] = pkt.input;
              pkt.opcode = OPCODE_ACK;
              pkt.epoch = epoch;
              pkt.input = 0;
              net_send(&pkt);
              break;
            case OPCODE_ACK:
              epoch_state.ack = true;
              break;
          }
        }
      }

      /* TODO: Update cmds[player] and set cmd_self in epoch_state if cmd_self
         is not set */
      
      if (!epoch_state.cmd_self) {
        if (e.up) {
          cmds[player] = CMD_UP;
        } else if (e.down) {
          cmds[player] = CMD_DOWN;
        } else {
          cmds[player] = CMD_NONE;
        }

        epoch_state.cmd_self = true;
      }

      /* TODO: Send a command packet. */
      pkt.opcode = OPCODE_CMD;
      pkt.epoch = epoch;
      pkt.input = cmds[player];
      net_send(&pkt);

      /* TODO: Add conditions for simulation. To simulate and move onto the next
         epoch, we must have received the command packet and the acknowledge
         packet from the other player. */

      if (epoch_state.cmd && epoch_state.ack) {
        uint32_t epoch_end_tick = win_tick();
        fprintf(stderr, "epoch %u took %u ms\n", (unsigned)epoch,
                (unsigned)(epoch_end_tick - epoch_start_tick));
        epoch_start_tick = epoch_end_tick;

        state = sim_update(&state, cmds, SIM_INTERVAL / 1000.f);
        //printf("epoch: %d\nplayer 0: %d\nplayer 1: %d\n", epoch, cmds[0], cmds[1]);
        ++epoch;
        epoch_state.cmd_self = epoch_state.cmd = epoch_state.ack = false;

        win_render(&state);
      }
    }
  }

  net_fini();
  win_fini();
  return 0;
}
