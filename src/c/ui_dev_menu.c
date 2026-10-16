#include "ui_dev_menu.h"
#include "game_state.h"

#if ENABLE_DEV_MENU

static Window *s_dev_window = NULL;
static Layer *s_dev_header_layer = NULL;
static MenuLayer *s_dev_menu_layer = NULL;

// Live Ticker Window state
static Window *s_ticker_window = NULL;
static Layer *s_ticker_layer = NULL;
static AppTimer *s_ticker_timer = NULL;
static bool s_ticker_up_held = false;
static bool s_ticker_down_held = false;

// -------------------------------------------------------------
// Live Continuous Ticker Window
// -------------------------------------------------------------
static void prv_ticker_timer_callback(void *data) {
  s_ticker_timer = NULL;
  bool changed = false;

  if (s_ticker_up_held) {
    game_state_dev_add_cents(100); // +$1.00 per tick
    changed = true;
  }
  if (s_ticker_down_held) {
    game_state_dev_add_unlock_points(1); // +1 point per tick
    changed = true;
  }

  if (changed) {
    if (s_ticker_layer) layer_mark_dirty(s_ticker_layer);
    if (s_dev_header_layer) layer_mark_dirty(s_dev_header_layer);
  }

  if (s_ticker_up_held || s_ticker_down_held) {
    s_ticker_timer = app_timer_register(100, prv_ticker_timer_callback, NULL);
  }
}

static void prv_ticker_up_down(ClickRecognizerRef recognizer, void *context) {
  s_ticker_up_held = true;
  game_state_dev_add_cents(100);
  if (s_ticker_layer) layer_mark_dirty(s_ticker_layer);
  if (s_dev_header_layer) layer_mark_dirty(s_dev_header_layer);
  if (!s_ticker_timer) {
    s_ticker_timer = app_timer_register(300, prv_ticker_timer_callback, NULL);
  }
}

static void prv_ticker_up_up(ClickRecognizerRef recognizer, void *context) {
  s_ticker_up_held = false;
}

static void prv_ticker_down_down(ClickRecognizerRef recognizer, void *context) {
  s_ticker_down_held = true;
  game_state_dev_add_unlock_points(1);
  if (s_ticker_layer) layer_mark_dirty(s_ticker_layer);
  if (s_dev_header_layer) layer_mark_dirty(s_dev_header_layer);
  if (!s_ticker_timer) {
    s_ticker_timer = app_timer_register(300, prv_ticker_timer_callback, NULL);
  }
}

static void prv_ticker_down_up(ClickRecognizerRef recognizer, void *context) {
  s_ticker_down_held = false;
}

static void prv_ticker_select_click(ClickRecognizerRef recognizer, void *context) {
  game_state_dev_add_cents(1000); // +$10.00
  game_state_dev_add_unlock_points(5); // +5 points
  vibes_short_pulse();
  if (s_ticker_layer) layer_mark_dirty(s_ticker_layer);
  if (s_dev_header_layer) layer_mark_dirty(s_dev_header_layer);
}

static void prv_ticker_click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, prv_ticker_up_down, prv_ticker_up_up, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, prv_ticker_down_down, prv_ticker_down_up, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_ticker_select_click);
}

static void prv_ticker_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Title
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "DEV TICKER",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 4, bounds.size.w, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Divider
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(10, 26), GPoint(bounds.size.w - 10, 26));

  // Current Balance & Points display card
  char bal_buf[32];
  char pts_buf[32];
  game_state_format_currency(game->data.currency, bal_buf, sizeof(bal_buf));
  snprintf(pts_buf, sizeof(pts_buf), "Points: %lu", (unsigned long)game->data.unlock_points);

  char stats_buf[96];
  snprintf(stats_buf, sizeof(stats_buf), "Bal: %s\n%s", bal_buf, pts_buf);

  graphics_context_set_text_color(ctx, GColorYellow);
  graphics_draw_text(ctx, stats_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(10, 32, bounds.size.w - 20, 52),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Instructions
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "^ Hold UP: +$1/tick\nv Hold DN: +1 Pt/tick\n* SELECT: +$10 & +5 Pts",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(10, bounds.size.h - 58, bounds.size.w - 20, 52),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_ticker_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_ticker_layer = layer_create(bounds);
  layer_set_update_proc(s_ticker_layer, prv_ticker_layer_update_proc);
  layer_add_child(window_layer, s_ticker_layer);
}

static void prv_ticker_window_unload(Window *window) {
  if (s_ticker_timer) {
    app_timer_cancel(s_ticker_timer);
    s_ticker_timer = NULL;
  }
  s_ticker_up_held = false;
  s_ticker_down_held = false;

  layer_destroy(s_ticker_layer);
  s_ticker_layer = NULL;
  window_destroy(s_ticker_window);
  s_ticker_window = NULL;

  if (s_dev_menu_layer) {
    menu_layer_reload_data(s_dev_menu_layer);
  }
}

static void prv_open_live_ticker(void) {
  if (s_ticker_window) return;
  s_ticker_window = window_create();
  window_set_click_config_provider(s_ticker_window, prv_ticker_click_config_provider);
  window_set_window_handlers(s_ticker_window, (WindowHandlers) {
    .load = prv_ticker_window_load,
    .unload = prv_ticker_window_unload,
  });
  window_stack_push(s_ticker_window, true);
}

// -------------------------------------------------------------
// Dev Menu Main Screen
// -------------------------------------------------------------
static void prv_dev_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  char bal_buf[24];
  game_state_format_currency(game->data.currency, bal_buf, sizeof(bal_buf));

  char text_buf[48];
  snprintf(text_buf, sizeof(text_buf), "DEV | %s | %luCRW",
           bal_buf, (unsigned long)game_state_get_global_crowns());

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 2, bounds.size.w - 8, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_dev_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 10;
}

static void prv_dev_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "+ 10 Cents", "Add 10¢ to balance", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "+ $1.00 (100¢)", "Add $1.00 to balance", NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "+ $10.00 (1,000¢)", "Add $10.00 to balance", NULL);
      break;
    case 3:
      menu_cell_basic_draw(ctx, cell_layer, "+ $100.00 (10k¢)", "Add $100.00 to balance", NULL);
      break;
    case 4:
      menu_cell_basic_draw(ctx, cell_layer, "+ 1 Unlock Point", "Add 1 point for unlocks", NULL);
      break;
    case 5:
      menu_cell_basic_draw(ctx, cell_layer, "+ 5 Unlock Points", "Add 5 points for unlocks", NULL);
      break;
    case 6:
      menu_cell_basic_draw(ctx, cell_layer, "+ 10 Unlock Points", "Add 10 points for unlocks", NULL);
      break;
    case 7:
      menu_cell_basic_draw(ctx, cell_layer, "Live Ticker Tool", "Hold buttons to rapid tick", NULL);
      break;
    case 8:
      menu_cell_basic_draw(ctx, cell_layer, "Unlock All 13 BG Colors", "Instantly unlock palette", NULL);
      break;
    case 9:
      menu_cell_basic_draw(ctx, cell_layer, "+ 1 Crown", "Award game completion (Crown)", NULL);
      break;
    default:
      break;
  }
}

static void prv_dev_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool ticked = false;

  switch (cell_index->row) {
    case 0:
      game_state_dev_add_cents(10);
      ticked = true;
      break;
    case 1:
      game_state_dev_add_cents(100);
      ticked = true;
      break;
    case 2:
      game_state_dev_add_cents(1000);
      ticked = true;
      break;
    case 3:
      game_state_dev_add_cents(10000);
      ticked = true;
      break;
    case 4:
      game_state_dev_add_unlock_points(1);
      ticked = true;
      break;
    case 5:
      game_state_dev_add_unlock_points(5);
      ticked = true;
      break;
    case 6:
      game_state_dev_add_unlock_points(10);
      ticked = true;
      break;
    case 7:
      prv_open_live_ticker();
      return;
    case 8:
      game->data.unlocked_bg_colors = UNLOCK_BG_COLORS_COUNT;
      game_state_save();
      ticked = true;
      break;
    case 9:
      game_state_dev_add_crown();
      ticked = true;
      break;
    default:
      break;
  }

  if (ticked) {
    if (game->data.option_vibration) {
      vibes_short_pulse();
    }
    if (s_dev_header_layer) {
      layer_mark_dirty(s_dev_header_layer);
    }
    menu_layer_reload_data(menu_layer);
  }
}

static void prv_dev_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int header_h = 24;

  s_dev_header_layer = layer_create(GRect(0, 0, bounds.size.w, header_h));
  layer_set_update_proc(s_dev_header_layer, prv_dev_header_update_proc);
  layer_add_child(window_layer, s_dev_header_layer);

  GRect menu_bounds = GRect(0, header_h, bounds.size.w, bounds.size.h - header_h);
  s_dev_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_dev_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_dev_get_num_rows,
    .draw_row = prv_dev_draw_row,
    .select_click = prv_dev_select_callback,
  });

  menu_layer_set_click_config_onto_window(s_dev_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_dev_menu_layer));
}

static void prv_dev_window_unload(Window *window) {
  layer_destroy(s_dev_header_layer);
  s_dev_header_layer = NULL;
  menu_layer_destroy(s_dev_menu_layer);
  s_dev_menu_layer = NULL;
  window_destroy(s_dev_window);
  s_dev_window = NULL;
}

void ui_dev_menu_show(void) {
  if (s_dev_window) return;
  s_dev_window = window_create();
  window_set_window_handlers(s_dev_window, (WindowHandlers) {
    .load = prv_dev_window_load,
    .unload = prv_dev_window_unload,
  });
  window_stack_push(s_dev_window, true);
}

#else

void ui_dev_menu_show(void) {
  // Developer menu omitted from release build
}

#endif
