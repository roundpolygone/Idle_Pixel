#pragma once

#include <pebble.h>

// Pixel grid view (main game canvas)
void ui_grid_init(Window *window);
void ui_grid_deinit(void);
void ui_grid_mark_dirty(void);
bool ui_grid_is_zooming(void);
void ui_grid_start_zoom_out(void);
void ui_grid_start_zoom_in(void);
void ui_grid_tick_zoom(uint32_t delta_ms);

// Clock face view
void ui_clock_show(void);
void ui_clock_mark_dirty(void);
bool ui_clock_is_down_held(void);

// Menus
typedef struct MenuNavConfig {
  Window *window;
  MenuLayer *menu_layer;
  uint16_t (*get_num_rows)(void);
  ClickConfigProvider original_provider;
  AppTimer *marquee_timer;
  int16_t marquee_offset;
  int16_t max_scroll;
  uint16_t last_selected_row;
  uint16_t last_selected_section;
  uint16_t pause_ticks;
  bool reset_after_pause;
} MenuNavConfig;

void ui_menu_bind_navigation(Window *window, MenuLayer *menu_layer, MenuNavConfig *cfg, uint16_t (*get_num_rows)(void));
void ui_menu_unbind_navigation(MenuNavConfig *cfg);

// Marquee Text & Cell Drawing
void ui_menu_draw_text_marquee(GContext *ctx, const char *text, GFont font, GRect box, GColor color, bool is_highlighted);
void ui_menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title, const char *subtitle, GBitmap *icon);

void ui_menu_show(void);
void ui_store_show(void);
void ui_store_refresh_if_visible(void);
void ui_options_show(void);

// Unlocks & Cosmetics
void ui_unlocks_show(void);

// Save slots manager
void ui_save_slots_show(void);

// Win celebration
void ui_win_show(void);
