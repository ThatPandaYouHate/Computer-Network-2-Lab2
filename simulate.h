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

#ifndef SIMULATE_H
#define SIMULATE_H

#include <stdint.h>

#define NPLAYER 2

typedef struct vec {
  float x, y;
} vec_t;

typedef struct paddle {
  vec_t pos;
  vec_t size;
  float speed;
} paddle_t;

typedef struct ball {
  float init_speed;
  vec_t pos;
  vec_t vel;
  float radius;
} ball_t;

typedef struct state {
  paddle_t paddle[NPLAYER];
  ball_t ball;
  vec_t bound;
} state_t;

typedef enum { CMD_NONE, CMD_UP, CMD_DOWN } cmd_t;

state_t sim_init(int width, int height);
state_t sim_update(const state_t *state, const cmd_t cmd[NPLAYER], float dt);

#endif
