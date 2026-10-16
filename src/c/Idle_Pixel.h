#pragma once
#include <pebble.h>

// Returns the root game window
Window* idle_pixel_get_main_window(void);

// Pops all pushed subwindows until only the root game window remains, keeping the app alive
void idle_pixel_pop_to_root(void);
