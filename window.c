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

#include "window.h"
#include "SDL_keycode.h"

#include <SDL.h>

static SDL_Window *window;
static SDL_Renderer *renderer;
static int win_width, win_height;
static win_event_t we;

void win_init(int width, int height) {
  SDL_Init(SDL_INIT_VIDEO);
  window = SDL_CreateWindow("xpong", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, width, height,
                            SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, 0);
  win_width = width;
  win_height = height;
}

void win_fini() {
  SDL_DestroyWindow(window);
  SDL_Quit();
}

win_event_t win_poll_event() {
  SDL_Event e;
  while (SDL_PollEvent(&e) != 0) {
    if (e.type == SDL_QUIT) {
      we.quit = true;
    } else if (e.type == SDL_KEYDOWN) {
      switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        we.quit = true;
      case SDLK_UP:
        we.up = true;
        break;
      case SDLK_DOWN:
        we.down = true;
        break;
      default:
        break;
      }
    } else if (e.type == SDL_KEYUP) {
      switch (e.key.keysym.sym) {
      case SDLK_ESCAPE:
        we.quit = false;
      case SDLK_UP:
        we.up = false;
        break;
      case SDLK_DOWN:
        we.down = false;
        break;
      default:
        break;
      }
    }
  }

  return we;
}

static vec_t vec_map(vec_t gpos) {
  vec_t spos = {gpos.x + win_width / 2, -gpos.y + win_height / 2};
  return spos;
}

static SDL_Rect create_rect(vec_t pos, vec_t size) {
  vec_t spos = vec_map(pos);
  SDL_Rect rect = {spos.x - size.x / 2, spos.y - size.y / 2, size.x, size.y};
  return rect;
}

static void render_paddle(SDL_Renderer *renderer, const paddle_t *paddle) {
  SDL_Rect rect = create_rect(paddle->pos, paddle->size);
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_RenderFillRect(renderer, &rect);
}

static void render_bounds(SDL_Renderer *renderer, vec_t bound) {
  vec_t size = {2 * bound.x, 10};
  vec_t up_pos = {0, bound.y + 5};
  SDL_Rect rect_up = create_rect(up_pos, size);
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_RenderFillRect(renderer, &rect_up);

  vec_t down_pos = {0, -bound.y - 5};
  SDL_Rect rect_down = create_rect(down_pos, size);
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_RenderFillRect(renderer, &rect_down);
}

static void render_ball(SDL_Renderer *renderer, const ball_t *ball) {
  vec_t size = {ball->radius * 2, ball->radius * 2};
  SDL_Rect rect = create_rect(ball->pos, size);
  SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
  SDL_RenderFillRect(renderer, &rect);
}

void win_render(const state_t *state) {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
  SDL_RenderClear(renderer);

  render_bounds(renderer, state->bound);

  for (size_t i = 0; i < NPLAYER; ++i) {
    render_paddle(renderer, &state->paddle[i]);
  }

  render_ball(renderer, &state->ball);

  SDL_RenderPresent(renderer);
}

uint32_t win_tick() { return SDL_GetTicks(); }
