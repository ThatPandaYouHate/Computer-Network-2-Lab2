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
#include <limits.h>
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
static const int BUFFER_SIZE = 64;
static const int CMD_DELAY = 10;


#define OPCODE_CMD 0
#define OPCODE_ACK 1

typedef struct cmd_state {
  int cmd_value;
  bool cmd_ack;
  int epoch;
} cmd_state_t;

static cmd_state_t cmd_state[2][BUFFER_SIZE] = {0};

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

  int received_pkt_count = 0;
  
  for (int i = 0; i < CMD_DELAY; i++) {
    cmd_state[player][i].cmd_value = 0;
    cmd_state[player][i].cmd_ack = false;
    cmd_state[player][i].epoch = i;
    cmd_state[other_player][i].cmd_value = 0;
    cmd_state[other_player][i].cmd_ack = false;
    cmd_state[other_player][i].epoch = i;
  }
  printf("cmd_state initialized\n");

  state_t state = sim_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  win_init(SCREEN_WIDTH, SCREEN_HEIGHT);
  net_init(port_self, hostname_other, port_other);

  uint16_t epoch = 0;
  cmd_t cmds[2];
  bool quit = false;

  uint32_t previous_tick = win_tick();
  uint32_t epoch_start_tick = previous_tick;
  
  // Statistics for epoch times
  uint32_t total_epoch_time = 0;
  uint32_t min_epoch_time = UINT32_MAX;
  uint32_t max_epoch_time = 0;
  uint16_t epoch_count = 0;
  
  // Array to store last 100 epoch times for averaging
  uint32_t last_100_epoch_times[100] = {0};
  uint16_t epoch_time_index = 0;

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
      
      while (net_poll(&pkt)) {
        received_pkt_count++;
        switch (pkt.opcode) {
          case OPCODE_CMD:
            cmd_state[other_player][pkt.epoch%BUFFER_SIZE].cmd_value = pkt.input;
            cmd_state[other_player][pkt.epoch%BUFFER_SIZE].epoch = pkt.epoch;
            pkt.opcode = OPCODE_ACK;
            pkt.input = 0;
            net_send(&pkt);
            break;
          case OPCODE_ACK:
            cmd_state[player][pkt.epoch%BUFFER_SIZE].cmd_ack = true;
            break;
          default:
            printf("received unknown packet from player %d\n", other_player);
            break;
        }
      }

      /* TODO: Update cmds[player] and set cmd_self in epoch_state if cmd_self
         is not set */
      
      
      if (e.up) {
        cmds[player] = CMD_UP;
      } else if (e.down) {
        cmds[player] = CMD_DOWN;
      } else {
        cmds[player] = CMD_NONE;
      }

      if (cmd_state[player][(epoch+CMD_DELAY) % BUFFER_SIZE].epoch != epoch + CMD_DELAY) {
        cmd_state[player][(epoch+CMD_DELAY)%BUFFER_SIZE].cmd_value = cmds[player];
        cmd_state[player][(epoch+CMD_DELAY)%BUFFER_SIZE].cmd_ack = false;
        cmd_state[player][(epoch+CMD_DELAY)%BUFFER_SIZE].epoch = epoch + CMD_DELAY;
        pkt.opcode = OPCODE_CMD;
        pkt.epoch = epoch + CMD_DELAY;
        pkt.input = cmds[player];
        net_send(&pkt);

      }

      for (int i = 0; i < (CMD_DELAY/2); i++) {
        if (cmd_state[player][(epoch + i) % BUFFER_SIZE].cmd_ack == false) {
          pkt.opcode = OPCODE_CMD;
          pkt.epoch = epoch + i;
          pkt.input = cmd_state[player][(epoch + i) % BUFFER_SIZE].cmd_value;
          net_send(&pkt);
          break;
        }
      }

      if (received_pkt_count > 0 &&cmd_state[other_player][epoch % BUFFER_SIZE].epoch == epoch && cmd_state[player][epoch % BUFFER_SIZE].cmd_ack == true) {
        uint32_t epoch_end_tick = win_tick();
        uint32_t epoch_time = epoch_end_tick - epoch_start_tick;
        
        // Collect statistics
        total_epoch_time += epoch_time;
        if (epoch_time < min_epoch_time) {
          min_epoch_time = epoch_time;
        }
        if (epoch_time > max_epoch_time) {
          max_epoch_time = epoch_time;
        }
        epoch_count++;
        
        // Store epoch time in circular buffer
        last_100_epoch_times[epoch_time_index] = epoch_time;
        epoch_time_index = (epoch_time_index + 1) % 100;
        
        // Print average time for every 100th epoch
        if (epoch > 0 && epoch % 100 == 0) {
          uint32_t sum = 0;
          uint16_t count = epoch_count < 100 ? epoch_count : 100;
          
          // Calculate average from circular buffer
          // If we have less than 100 epochs, use all available
          // Otherwise, use the last 100 (which wraps around)
          if (epoch_count < 100) {
            for (uint16_t i = 0; i < count; i++) {
              sum += last_100_epoch_times[i];
            }
          } else {
            // Circular buffer: values from epoch_time_index to end, then from start to epoch_time_index
            for (uint16_t i = epoch_time_index; i < 100; i++) {
              sum += last_100_epoch_times[i];
            }
            for (uint16_t i = 0; i < epoch_time_index; i++) {
              sum += last_100_epoch_times[i];
            }
          }
          uint32_t avg_time = sum / count;
          fprintf(stderr, "epoch %u: average time over last %u epochs: %u ms\n", 
                  (unsigned)epoch, count, avg_time);
        }
        
        epoch_start_tick = epoch_end_tick;

        cmds[other_player] = cmd_state[other_player][epoch % BUFFER_SIZE].cmd_value;
        cmds[player] = cmd_state[player][epoch % BUFFER_SIZE].cmd_value;
        state = sim_update(&state, cmds, SIM_INTERVAL / 1000.f);
        win_render(&state);
        ++epoch;

      }
      
    }
  }

  // Print summary of epoch times
  if (epoch_count > 0) {
    uint32_t avg_epoch_time = total_epoch_time / epoch_count;
    fprintf(stderr, "\n=== Epoch Time Summary ===\n");
    fprintf(stderr, "Total epochs: %u\n", epoch_count);
    fprintf(stderr, "Total time: %u ms\n", total_epoch_time);
    fprintf(stderr, "Average time per epoch: %u ms\n", avg_epoch_time);
    fprintf(stderr, "Minimum time: %u ms\n", min_epoch_time);
    fprintf(stderr, "Maximum time: %u ms\n", max_epoch_time);
    fprintf(stderr, "========================\n");
  }

  net_fini();
  win_fini();
  return 0;
}
