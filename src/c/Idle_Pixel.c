#include <pebble.h>
#include "game_state.h"
#include "ui_grid.h"
#include "ui_menu.h"
#include "ui_clock.h"
#include "ui_offline_summary.h"

static Window *s_window;
static AppTimer *s_tick_timer = NULL;

static void prv_timer_callback(void *data) {
  bool pixel_added = false;
  bool fill_completed = false;

  game_state_tick(TICK_INTERVAL_MS, &pixel_added, &fill_completed);

  GameState *game = game_state_get();
  if (fill_completed && game->data.option_vibration) {
    vibes_short_pulse();
  }

  ui_grid_mark_dirty();

  s_tick_timer = app_timer_register(TICK_INTERVAL_MS, prv_timer_callback, NULL);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Shortcut to clock page
  ui_clock_show();
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  ui_menu_show();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  game_state_activate_boost();
  ui_grid_mark_dirty();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  ui_grid_init(window);
}

static void prv_window_unload(Window *window) {
  ui_grid_deinit();
}

static void prv_init(void) {
  game_state_init();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = true;
  window_stack_push(s_window, animated);

  // If closed > 3 minutes (and not first time opened), show preliminary summary
  ui_offline_summary_show_if_needed();

  s_tick_timer = app_timer_register(TICK_INTERVAL_MS, prv_timer_callback, NULL);
}

static void prv_deinit(void) {
  if (s_tick_timer) {
    app_timer_cancel(s_tick_timer);
    s_tick_timer = NULL;
  }

  game_state_save();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
