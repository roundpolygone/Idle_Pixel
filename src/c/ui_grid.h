#pragma once

#include <pebble.h>

void ui_grid_init(Window *window);
void ui_grid_deinit(void);
void ui_grid_mark_dirty(void);

// Smooth zoom transitions between Tier 1 and Tier 2
void ui_grid_start_zoom_out(void);
void ui_grid_start_zoom_in(void);
bool ui_grid_is_zooming(void);
void ui_grid_tick_zoom(uint32_t dt_ms);
