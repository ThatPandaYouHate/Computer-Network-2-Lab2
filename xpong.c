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

/* Game display constants */
static const int SCREEN_WIDTH = 720;
static const int SCREEN_HEIGHT = 640;
static const int SIM_INTERVAL = 10;  /* Simulation interval in milliseconds */

/* Network protocol constants */
static const int BUFFER_SIZE = 64;   /* Circular buffer size for command state */
static const int CMD_DELAY = 25;     /* Number of epochs ahead to send commands */

/* Packet opcodes */
#define OPCODE_CMD 0  /* Command packet: contains player input */
#define OPCODE_ACK 1  /* Acknowledgment packet: confirms receipt of command */

/* Buffer index macros for circular buffer access
 * These macros calculate the correct index in the circular buffer for a given epoch */
#define PLANNED_EPOCH_IN_BUFFER(epoch) ((epoch) + CMD_DELAY) % BUFFER_SIZE  /* Index for future epoch (epoch + delay) */
#define CURRENT_EPOCH_IN_BUFFER(epoch) (epoch) % BUFFER_SIZE                /* Index for current epoch */

/* Command state structure
 * Tracks the state of commands for each epoch in the circular buffer */
typedef struct cmd_state {
  int cmd_value;   /* The command value (CMD_NONE, CMD_UP, or CMD_DOWN) */
  bool cmd_ack;    /* Whether this command has been acknowledged by the peer */
  int epoch;       /* The epoch number this state belongs to */
} cmd_state_t;

/* Command state buffer: [player_index][buffer_index]
 * Stores command states for both players in a circular buffer */
static cmd_state_t cmd_state[2][BUFFER_SIZE] = {0};

/**
 * Print usage information and exit
 * @param program_name Name of the program executable
 */
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
  /* Parse command line arguments */
  unsigned short port_self = atoi(argv[1]);
  const char *hostname_other = argv[2];
  unsigned short port_other = atoi(argv[3]);
  int player = atol(argv[4]);
  int other_player = player == 0 ? 1 : 0;

  /* Network state tracking */
  int received_pkt_count = 0;  /* Counter for received packets */
  bool resend_done = false;     /* Flag to prevent duplicate resends in same iteration */
  
  /* Initialize command state buffer for the first CMD_DELAY epochs
   * This pre-fills the buffer with initial state to handle the delay mechanism */
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

  /* Game state variables */
  uint16_t epoch = 0;           /* Current epoch number */
  cmd_t cmds[2];                /* Commands for both players in current epoch */
  bool quit = false;            /* Flag to exit main loop */
  int loop_count = 0;           /* Counter for periodic operations */
  bool in_sync = true;          /* Whether both players are synchronized */

  /* Timing variables */
  uint32_t previous_tick = win_tick();      /* Previous simulation tick */
  uint32_t epoch_start_tick = previous_tick; /* Start time of current epoch */
  
  /* Performance statistics */
  uint32_t total_epoch_time = 0;      /* Sum of all epoch durations */
  uint32_t min_epoch_time = UINT32_MAX; /* Minimum epoch duration */
  uint32_t max_epoch_time = 0;         /* Maximum epoch duration */
  uint16_t epoch_count = 0;            /* Total number of completed epochs */
  
  /* Circular buffer for recent epoch times (for averaging) */
  uint32_t last_100_epoch_times[100] = {0};
  uint16_t epoch_time_index = 0;  /* Current position in circular buffer */

  printf("game started\n");
  printf("waiting for player %d to start the game\n", other_player);
  while (!quit) {
    net_packet_t pkt;
    win_event_t e = win_poll_event();
    if (e.quit)
      quit = true;

    /* Simulation loop: runs every SIM_INTERVAL milliseconds */
    for (; win_tick() - previous_tick > SIM_INTERVAL;
        previous_tick += SIM_INTERVAL) {
      
      /* Packet reception and processing
       * Poll for incoming packets and handle them according to protocol:
       * - CMD packets: Store command, send ACK response
       * - ACK packets: Mark corresponding command as acknowledged */
      while (net_poll(&pkt)) {
        received_pkt_count++;
        switch (pkt.opcode) {
          case OPCODE_CMD:
            /* Received command from other player: store it and send ACK */
            cmd_state[other_player][CURRENT_EPOCH_IN_BUFFER(pkt.epoch)].cmd_value = pkt.input;
            cmd_state[other_player][CURRENT_EPOCH_IN_BUFFER(pkt.epoch)].epoch = pkt.epoch;
            /* Convert packet to ACK and send response */
            pkt.opcode = OPCODE_ACK;
            pkt.input = 0;
            net_send(&pkt);
            break;
          case OPCODE_ACK:
            /* Received acknowledgment: mark our command as acknowledged */
            cmd_state[player][CURRENT_EPOCH_IN_BUFFER(pkt.epoch)].cmd_ack = true;
            break;
          default:
            printf("received unknown packet from player %d\n", other_player);
            break;
        }
      }

      /* Read player input and update command for current epoch */
      if (e.up) {
        cmds[player] = CMD_UP;
      } else if (e.down) {
        cmds[player] = CMD_DOWN;
      } else {
        cmds[player] = CMD_NONE;
      }

      /* Command delay mechanism: send commands CMD_DELAY epochs ahead
       * This allows commands to arrive in time despite network latency.
       * Only send if we haven't already sent a command for this future epoch */
      if (cmd_state[player][PLANNED_EPOCH_IN_BUFFER(epoch)].epoch != epoch + CMD_DELAY) {
        /* Store command in buffer for future epoch */
        cmd_state[player][PLANNED_EPOCH_IN_BUFFER(epoch)].cmd_value = cmds[player];
        cmd_state[player][PLANNED_EPOCH_IN_BUFFER(epoch)].cmd_ack = false;
        cmd_state[player][PLANNED_EPOCH_IN_BUFFER(epoch)].epoch = epoch + CMD_DELAY;
        /* Send command packet for future epoch */
        pkt.opcode = OPCODE_CMD;
        pkt.epoch = epoch + CMD_DELAY;
        pkt.input = cmds[player];
        net_send(&pkt);
      }

      /* Periodic reset: allow resend after 100 loop iterations
       * This prevents infinite waiting if packets are lost */
      if (loop_count > 100) {
        loop_count = 0;
        resend_done = false;
      }

      /* Resend mechanism: retransmit unacknowledged commands
       * Use adaptive resend limit based on sync status:
       * - When in sync: only resend recent epochs (CMD_DELAY/2)
       * - When out of sync: resend more epochs (CMD_DELAY) to recover faster */
      int resend_limit = CMD_DELAY / 2;
      if (in_sync == false) {
        resend_limit = CMD_DELAY;
      }

      /* Resend unacknowledged commands
       * Only resend if:
       * 1. Command hasn't been acknowledged
       * 2. We haven't already done resend this iteration (resend_done == false)
       *    OR it's the first epoch (epoch == 0) */
      for (int i = 0; i < resend_limit; i++) {
        if (cmd_state[player][(epoch + i) % BUFFER_SIZE].cmd_ack == false && 
            (resend_done == false || epoch == 0)) {
          pkt.opcode = OPCODE_CMD;
          pkt.epoch = epoch + i;
          pkt.input = cmd_state[player][(epoch + i) % BUFFER_SIZE].cmd_value;
          net_send(&pkt);
        }
      }
      resend_done = true;  /* Mark that resend has been done for this iteration */

      /* Sync detection: check if we've received the delayed command from other player
       * This indicates that the other player is sending commands ahead of time */
      if (in_sync == false && cmd_state[other_player][PLANNED_EPOCH_IN_BUFFER(epoch)].epoch == epoch + CMD_DELAY) {
        in_sync = true;
      }
      /* Epoch advancement condition: can we proceed to next epoch?
       * Requirements:
       * 1. At least one packet has been received (connection established)
       * 2. We have received command from other player for current epoch
       * 3. Our command for current epoch has been acknowledged
       * 4. Both players are synchronized */
      if (received_pkt_count > 0 && 
          cmd_state[other_player][CURRENT_EPOCH_IN_BUFFER(epoch)].epoch == epoch && 
          cmd_state[player][CURRENT_EPOCH_IN_BUFFER(epoch)].cmd_ack == true && 
          in_sync == true) {
        
        /* Calculate epoch duration */
        uint32_t epoch_end_tick = win_tick();
        uint32_t epoch_time = epoch_end_tick - epoch_start_tick;
        
        /* Update performance statistics */
        total_epoch_time += epoch_time;
        if (epoch_time < min_epoch_time) {
          min_epoch_time = epoch_time;
        }
        if (epoch_time > max_epoch_time) {
          max_epoch_time = epoch_time;
        }
        epoch_count++;
        
        /* Store epoch time in circular buffer for averaging */
        last_100_epoch_times[epoch_time_index] = epoch_time;
        epoch_time_index = (epoch_time_index + 1) % 100;
        
        /* Print average time every 100 epochs for monitoring */
        if (epoch > 0 && epoch % 100 == 0) {
          uint32_t sum = 0;
          uint16_t count = epoch_count < 100 ? epoch_count : 100;
          
          /* Calculate average from circular buffer
           * Handle two cases:
           * 1. Less than 100 epochs: use all available data
           * 2. 100+ epochs: use last 100 (circular buffer wraps around) */
          if (epoch_count < 100) {
            for (uint16_t i = 0; i < count; i++) {
              sum += last_100_epoch_times[i];
            }
          } else {
            /* Circular buffer access: read from current index to end, then from start to index */
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
        
        epoch_start_tick = epoch_end_tick;  /* Reset timer for next epoch */

        /* Retrieve commands for both players from buffer */
        cmds[other_player] = cmd_state[other_player][CURRENT_EPOCH_IN_BUFFER(epoch)].cmd_value;
        cmds[player] = cmd_state[player][CURRENT_EPOCH_IN_BUFFER(epoch)].cmd_value;
        
        /* Update game simulation and render */
        state = sim_update(&state, cmds, SIM_INTERVAL / 1000.f);
        win_render(&state);
        
        /* Advance to next epoch and reset resend flag */
        ++epoch;
        resend_done = false;
      }
      else {
        /* Epoch not ready: waiting for synchronization or acknowledgments */
        if (in_sync == true) {
          printf("epoch %u not ready\n", epoch);
        }
        /* Mark as out of sync if we're past the initial epoch */
        if (epoch > 0) {
          in_sync = false;
        }
      }
      loop_count++;
    }
  }

  /* Print performance summary when program exits */
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
