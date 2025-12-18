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
static const int BUFFER_SIZE = 64;  // Add buffer size


#define OPCODE_CMD 0
#define OPCODE_ACK 1
#define OPCODE_START 2

typedef struct epoch {
  bool cmd;
  bool ack;
  bool cmd_self;
} epoch_t;

int main(int argc, char *argv[argc + 1]) {
  unsigned short port_self = atoi(argv[1]);  /* 9930 */
  const char *hostname_other = argv[2];      /* "127.0.0.1" */
  unsigned short port_other = atoi(argv[3]); /* 9931 */
  int player = atol(argv[4]);                /* 0 */
  int other_player = player == 0 ? 1 : 0;
  bool game_started = false;
  
  state_t state = sim_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  win_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  net_init(port_self, hostname_other, port_other);

  uint16_t epoch = 0;
  epoch_t epoch_state = {false, false, false};
  cmd_t cmds[2];
  cmd_t cmd_buffer[2][BUFFER_SIZE];  // ADD: Command buffer
  bool quit = false;

  uint32_t previous_tick = win_tick();

  while (!quit) {
    net_packet_t pkt;
    win_event_t e = win_poll_event();
    if (e.quit)
      quit = true;

    /* Wait for the other player to start the game */
    while (!game_started) {
      pkt.opcode = OPCODE_START;
      pkt.epoch = epoch;
      pkt.input = 0;
      net_send(&pkt);
      if (net_poll(&pkt)){
        if (pkt.opcode == OPCODE_START) {
          game_started = true;
          printf("game started\n");
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

    /*
     * TODO: Poll and handle each packet until no more packet.
     *
     * If we receive a command packet, send an acknowledgement packet, mark
     * its flag in epoch_state, and set the command in cmds array. If we
     * receive a acknowledge packet, just mark its flag in epoch_state.
     */
    
     
    while (net_poll(&pkt)) {
      if (pkt.opcode == OPCODE_CMD && pkt.epoch == epoch && !epoch_state.cmd) {
        epoch_state.cmd = true;
        cmds[other_player] = pkt.input;
        net_packet_t ack_pkt;
        ack_pkt.opcode = OPCODE_ACK;
        ack_pkt.epoch = epoch;
        ack_pkt.input = 0;
        net_send(&ack_pkt);
      } else if (pkt.opcode == OPCODE_ACK && pkt.epoch == epoch) {
        epoch_state.ack = true;
      }
    }

    /* TODO: Send a command packet. */
    if (epoch_state.cmd_self) {
      net_packet_t cmd_pkt;
      cmd_pkt.opcode = OPCODE_CMD;
      cmd_pkt.epoch = epoch;
      cmd_pkt.input = cmds[player];
      net_send(&cmd_pkt);
    }

    for (; win_tick() - previous_tick > SIM_INTERVAL;
        previous_tick += SIM_INTERVAL) {

      /* TODO: Add conditions for simulation. To simulate and move onto the next
         epoch, we must have received the command packet and the acknowledge
         packet from the other player. */

      if (epoch_state.cmd && epoch_state.ack) {
        cmd_buffer[player][(epoch + 10) % BUFFER_SIZE] = cmds[player];
        cmd_buffer[other_player][(epoch + 10) % BUFFER_SIZE] = cmds[other_player];

        printf("[RECORD] epoch %d: storing cmds for epoch %d -> player %d: %d, player %d: %d\n",
               epoch, (epoch + 10) % BUFFER_SIZE, player, cmds[player], other_player, cmds[other_player]);

        if (epoch >= 10) {
          cmd_t epoch_cmds[2] = {
            cmd_buffer[0][epoch % BUFFER_SIZE],
            cmd_buffer[1][epoch % BUFFER_SIZE]
          };
          printf("[EXECUTE] epoch %d: executing from buffer slot %d -> player 0: %d, player 1: %d\n",
                 epoch, epoch % BUFFER_SIZE, epoch_cmds[0], epoch_cmds[1]);
          state = sim_update(&state, epoch_cmds, SIM_INTERVAL / 1000.f);
        }
        printf("epoch: %d\nplayer 0: %d\nplayer 1: %d\n", epoch, cmds[0], cmds[1]);
        ++epoch;
        epoch_state.cmd_self = epoch_state.cmd = epoch_state.ack = false;
      }
    }

    /* Render every frame, not just when synchronized */
    win_render(&state);
  }

  net_fini();
  win_fini();
  return 0;
}
