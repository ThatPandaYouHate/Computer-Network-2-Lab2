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

#ifndef RENDER_H
#define RENDER_H

#include "simulate.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct win_event {
  bool quit, up, down;
} win_event_t;

win_event_t win_poll_event();

void win_init(int width, int height);

void win_fini();

void win_render(const state_t *state);

/* Return ticks in milliseconds */
uint32_t win_tick();

#endif
