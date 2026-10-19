#include "Idle_Pixel.h"
#include "game_state.h"
#include "ui.h"

static Window *s_window;
static AppTimer *s_tick_timer = NULL;
static bool s_down_held = false;
static uint32_t s_down_hold_timer_ms = 0;

Window* idle_pixel_get_main_window(void) {
  return s_window;
}

void idle_pixel_pop_to_root(void) {
  while (true) {
    Window *top = window_stack_get_top_window();
    if (!top || top == s_window) {
      break;
    }
    window_stack_pop(false);
  }
  ui_grid_mark_dirty();
}

static void prv_timer_callback(void *data) {
  GameState *game = game_state_get();
  uint32_t tick_interval = (game_state_is_meld_anim_running() || ui_grid_is_zooming() || (game->current_boost_milli > 0)) ? 33 : TICK_INTERVAL_MS;
  bool pixel_added = false;
  bool fill_completed = false;

  // Advance zoom transition if active
  if (ui_grid_is_zooming()) {
    ui_grid_tick_zoom(tick_interval);
  }

  // Handle auto-press boost while Down button is held (on main view or clock face)
  bool down_held = (s_down_held && game_state_get_view_mode() == VIEW_MODE_TIER1) ||
                   (ui_clock_is_down_held() && game_state_is_clock_active());
  if (down_held) {
    s_down_hold_timer_ms += tick_interval;
    uint32_t hold_interval = game_state_get_hold_boost_interval_ms();
    while (s_down_hold_timer_ms >= hold_interval) {
      s_down_hold_timer_ms -= hold_interval;
      game_state_activate_boost();
    }
  }

  // Handle Crown Auto Boost (starts at 5s, -1s per level down to 1s)
  uint32_t auto_boost_interval = game_state_get_auto_boost_interval_ms();
  if (auto_boost_interval > 0) {
    static uint32_t s_auto_boost_timer_ms = 0;
    s_auto_boost_timer_ms += tick_interval;
    if (s_auto_boost_timer_ms >= auto_boost_interval) {
      s_auto_boost_timer_ms = 0;
      game_state_activate_boost();
    }
  }

  game_state_tick(tick_interval, &pixel_added, &fill_completed);

  if (fill_completed && game->data.option_vibration) {
    vibes_short_pulse();
  }

  // Check if win game celebration was triggered
  if (game_state_check_and_clear_win_trigger()) {
    ui_win_show();
  }

  if (game_state_is_clock_active()) {
    if (game->current_boost_milli > 0) {
      ui_clock_mark_dirty();
    }
  } else {
    ui_grid_mark_dirty();
  }

  s_tick_timer = app_timer_register(tick_interval, prv_timer_callback, NULL);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Up button smoothly zooms out to Tier 2 grid
  if (game_state_get_view_mode() == VIEW_MODE_TIER1) {
    ui_grid_start_zoom_out();
  }
}

static void prv_up_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
  // Hold UP for clock face
  ui_clock_show();
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  ui_menu_show();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // On Tier 2 screen, down zooms into Tier 1 grid
  if (game_state_get_view_mode() == VIEW_MODE_TIER2) {
    ui_grid_start_zoom_in();
    return;
  }
  // On Tier 1 screen, down activates boost
  game_state_activate_boost();
  ui_grid_mark_dirty();
}

static void prv_down_long_click_down_handler(ClickRecognizerRef recognizer, void *context) {
  if (game_state_get_view_mode() == VIEW_MODE_TIER2) {
    ui_grid_start_zoom_in();
    return;
  }
  s_down_held = true;
  s_down_hold_timer_ms = 0;
  game_state_activate_boost();
  ui_grid_mark_dirty();
}

static void prv_down_long_click_up_handler(ClickRecognizerRef recognizer, void *context) {
  s_down_held = false;
  s_down_hold_timer_ms = 0;
}

static void prv_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  // On Tier 2 screen, back zooms into Tier 1 grid
  if (game_state_get_view_mode() == VIEW_MODE_TIER2) {
    ui_grid_start_zoom_in();
    return;
  }
  window_stack_pop(true);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click_handler);

  // UP button: Single click to zoom out, hold for Clock
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 500, prv_up_long_click_down_handler, NULL);

  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);

  // DOWN button: Single click to boost (or zoom in), hold for Auto-Boost
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 400, prv_down_long_click_down_handler, prv_down_long_click_up_handler);
}

static void prv_window_load(Window *window) {
  ui_grid_init(window);
}

static void prv_window_unload(Window *window) {
  ui_grid_deinit();
}

static void prv_accel_tap_handler(AccelAxisType axis, int32_t direction) {
  light_enable_interaction();
}

static void prv_init(void) {
  game_state_init();

#if defined(_PBL_API_EXISTS_app_touch_navigation_enable)
  app_touch_navigation_enable(true);
#endif

  accel_tap_service_subscribe(prv_accel_tap_handler);

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  const bool animated = true;
  window_stack_push(s_window, animated);

  s_tick_timer = app_timer_register(TICK_INTERVAL_MS, prv_timer_callback, NULL);
}

static void prv_deinit(void) {
  accel_tap_service_unsubscribe();

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
