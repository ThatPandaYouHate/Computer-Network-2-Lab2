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

#include "simulate.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

static float normal(float x) { return x > 0 ? 1 : -1; }

static paddle_t move_paddle(paddle_t paddle, vec_t bound, cmd_t cmd, float dt) {
  float cmd_speed[] = {0, paddle.speed, -paddle.speed};
  paddle.pos.y += cmd_speed[cmd] * dt;
  if (fabsf(paddle.pos.y) + paddle.size.y / 2 > bound.y) {
    paddle.pos.y = normal(paddle.pos.y) * bound.y -
                   normal(paddle.pos.y) * paddle.size.y / 2;
  }
  return paddle;
}

static ball_t move_ball(ball_t ball, paddle_t paddle[2], vec_t bound,
                        float dt) {
  ball.pos.x += ball.vel.x * dt;
  ball.pos.y += ball.vel.y * dt;

  if (fabsf(ball.pos.y) + ball.radius > bound.y) {
    ball.pos.y =
        normal(ball.pos.y) * bound.y - normal(ball.pos.y) * ball.radius;
    ball.vel.y = -ball.vel.y;
  }

  if (!ball.pos.x)
    return ball;

  paddle_t p = paddle[ball.pos.x > 0];
  if (p.pos.y - p.size.y / 2 < ball.pos.y &&
      ball.pos.y < p.pos.y + p.size.y / 2)
    if (fabsf(p.pos.x) - p.size.x / 2 - ball.radius < fabsf(ball.pos.x) &&
        fabsf(ball.pos.x) < fabsf(p.pos.x) + p.size.x / 2 + ball.radius) {
      ball.pos.x = p.pos.x - normal(ball.pos.x) * ball.radius -
                   normal(ball.pos.x) * p.size.x / 2;

      ball.vel.x *= -1;

      float hit_pos = p.pos.y - ball.pos.y;
      ball.vel.y = -hit_pos / p.size.y * ball.init_speed * 2;
    }

  if (fabsf(ball.pos.x) > bound.x) {
    ball.pos.x = 0;
    ball.pos.y = 0;
    ball.vel.x = normal(ball.vel.x) * ball.init_speed;
    ball.vel.y = 0;
  }

  return ball;
}

state_t sim_init(int width, int height) {
  ball_t ball = {.init_speed = 300,
                 .pos = {0, 0},
                 .vel = {ball.init_speed, 0},
                 .radius = 10};
  paddle_t paddle0 = {
      .pos = {-width / 2 + 50, 0}, .size = {20, 100}, .speed = 400};
  paddle_t paddle1 = paddle0;
  paddle1.pos.x *= -1;
  vec_t bound = {width / 2 - 20, height / 2 - 20};

  state_t state =
      (state_t){.paddle = {paddle0, paddle1}, .ball = ball, .bound = bound};
  return state;
}

state_t sim_update(const state_t *state0, const cmd_t cmd[NPLAYER], float dt) {
  state_t state = *state0;
  for (size_t i = 0; i < NPLAYER; ++i) {
    state.paddle[i] = move_paddle(state.paddle[i], state.bound, cmd[i], dt);
  }

  state.ball = move_ball(state.ball, state.paddle, state.bound, dt);

  return state;
}
