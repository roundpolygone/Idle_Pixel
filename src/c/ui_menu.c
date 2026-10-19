#include "ui.h"
#include "game_state.h"
#include "Idle_Pixel.h"

// =============================================================
// Window & Layer References
// =============================================================
static Window *s_main_menu_window = NULL;
static MenuLayer *s_main_menu_layer = NULL;

static Window *s_store_window = NULL;
static Layer *s_store_header_layer = NULL;
static MenuLayer *s_store_menu_layer = NULL;

static Window *s_options_window = NULL;
static MenuLayer *s_options_menu_layer = NULL;

static Window *s_reset_window = NULL;
static Layer *s_reset_content_layer = NULL;

static Window *s_prestige_confirm_window = NULL;
static MenuLayer *s_prestige_confirm_menu = NULL;

static MenuNavConfig s_main_menu_nav_cfg;
static MenuNavConfig s_store_menu_nav_cfg;
static MenuNavConfig s_options_menu_nav_cfg;
static MenuNavConfig s_prestige_confirm_nav_cfg;

#define MARQUEE_TICK_MS 50
#define MARQUEE_START_PAUSE_TICKS 25
#define MARQUEE_END_PAUSE_TICKS 25

static MenuNavConfig *s_active_nav_cfg = NULL;

void ui_menu_draw_text_marquee(GContext *ctx, const char *text, GFont font, GRect box, GColor color, bool is_highlighted) {
  if (!text || !text[0]) return;

  if (!is_highlighted) {
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    return;
  }

  GSize size = graphics_text_layout_get_content_size(text, font, GRect(0, 0, 2000, box.size.h + 10), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  if (size.w <= box.size.w) {
    graphics_context_set_text_color(ctx, color);
    graphics_draw_text(ctx, text, font, box, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    return;
  }

  int max_scroll = size.w - box.size.w + 12;
  if (s_active_nav_cfg && max_scroll > s_active_nav_cfg->max_scroll) {
    s_active_nav_cfg->max_scroll = max_scroll;
  }

  int offset = (s_active_nav_cfg != NULL) ? s_active_nav_cfg->marquee_offset : 0;
  if (offset > max_scroll) offset = max_scroll;

  GRect draw_box = GRect(box.origin.x - offset, box.origin.y, size.w + 20, box.size.h);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, draw_box, GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

void ui_menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title,
                             const char *subtitle, GBitmap *icon) {
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
  if (!is_highlighted) {
    menu_cell_basic_draw(ctx, cell_layer, title, subtitle, icon);
    return;
  }

  GRect bounds = layer_get_bounds(cell_layer);
  int left_margin = (icon != NULL) ? 36 : PBL_IF_ROUND_ELSE(18, 6);
  int right_margin = PBL_IF_ROUND_ELSE(18, 6);
  int avail_w = bounds.size.w - left_margin - right_margin;

  bool has_sub = (subtitle != NULL && subtitle[0] != '\0');
  GFont title_font = fonts_get_system_font(has_sub ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_28_BOLD);
  GFont sub_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  GSize title_size = (title && title[0]) ?
      graphics_text_layout_get_content_size(title, title_font, GRect(0, 0, 2000, 30), GTextOverflowModeWordWrap, GTextAlignmentLeft) :
      (GSize){0, 0};
  GSize sub_size = has_sub ?
      graphics_text_layout_get_content_size(subtitle, sub_font, GRect(0, 0, 2000, 30), GTextOverflowModeWordWrap, GTextAlignmentLeft) :
      (GSize){0, 0};

  bool title_truncated = (title && title_size.w > avail_w);
  bool sub_truncated = (has_sub && sub_size.w > avail_w);

  if (!title_truncated && !sub_truncated) {
    menu_cell_basic_draw(ctx, cell_layer, title, subtitle, icon);
    if (s_active_nav_cfg) {
      s_active_nav_cfg->max_scroll = 0;
    }
    return;
  }

  menu_cell_basic_draw(ctx, cell_layer, "", "", icon);

  if (has_sub) {
    int y_title = (bounds.size.h >= 50) ? 3 : 1;
    int y_sub = (bounds.size.h >= 50) ? 27 : 23;
    int h_title = y_sub - y_title;
    int h_sub = bounds.size.h - y_sub;

    if (title && title[0]) {
      GRect title_box = GRect(left_margin, y_title, avail_w, h_title);
      ui_menu_draw_text_marquee(ctx, title, title_font, title_box, GColorWhite, true);
    }

    GRect sub_box = GRect(left_margin, y_sub, avail_w, h_sub);
    ui_menu_draw_text_marquee(ctx, subtitle, sub_font, sub_box, GColorWhite, true);
  } else {
    if (title && title[0]) {
      int y_title = (bounds.size.h - 28) / 2;
      GRect title_box = GRect(left_margin, y_title, avail_w, 28);
      ui_menu_draw_text_marquee(ctx, title, title_font, title_box, GColorWhite, true);
    }
  }
}

static void prv_marquee_timer_callback(void *data) {
  MenuNavConfig *cfg = (MenuNavConfig*)data;
  if (!cfg || !cfg->window || !cfg->menu_layer) return;
  if (!window_stack_contains_window(cfg->window)) {
    cfg->marquee_timer = NULL;
    return;
  }
  if (window_stack_get_top_window() != cfg->window) {
    cfg->marquee_timer = app_timer_register(250, prv_marquee_timer_callback, cfg);
    return;
  }

  s_active_nav_cfg = cfg;

  MenuIndex sel = menu_layer_get_selected_index(cfg->menu_layer);
  if (sel.row != cfg->last_selected_row || sel.section != cfg->last_selected_section) {
    cfg->last_selected_row = sel.row;
    cfg->last_selected_section = sel.section;
    cfg->marquee_offset = 0;
    cfg->max_scroll = 0;
    cfg->pause_ticks = MARQUEE_START_PAUSE_TICKS;
    cfg->reset_after_pause = false;
    layer_mark_dirty(menu_layer_get_layer(cfg->menu_layer));
    cfg->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_timer_callback, cfg);
    return;
  }

  if (cfg->max_scroll <= 0) {
    cfg->marquee_offset = 0;
    cfg->marquee_timer = app_timer_register(100, prv_marquee_timer_callback, cfg);
    return;
  }

  if (cfg->pause_ticks > 0) {
    cfg->pause_ticks--;
    if (cfg->pause_ticks == 0 && cfg->reset_after_pause) {
      cfg->reset_after_pause = false;
      cfg->marquee_offset = 0;
      cfg->pause_ticks = MARQUEE_START_PAUSE_TICKS;
      layer_mark_dirty(menu_layer_get_layer(cfg->menu_layer));
    }
    cfg->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_timer_callback, cfg);
    return;
  }

  cfg->marquee_offset++;
  if (cfg->marquee_offset >= cfg->max_scroll) {
    cfg->marquee_offset = cfg->max_scroll;
    cfg->pause_ticks = MARQUEE_END_PAUSE_TICKS;
    cfg->reset_after_pause = true;
  }

  layer_mark_dirty(menu_layer_get_layer(cfg->menu_layer));
  cfg->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_timer_callback, cfg);
}

static void prv_menu_nav_reset_marquee(MenuNavConfig *cfg) {
  if (!cfg) return;
  cfg->marquee_offset = 0;
  cfg->max_scroll = 0;
  cfg->pause_ticks = MARQUEE_START_PAUSE_TICKS;
  cfg->reset_after_pause = false;
  if (cfg->menu_layer) {
    MenuIndex sel = menu_layer_get_selected_index(cfg->menu_layer);
    cfg->last_selected_row = sel.row;
    cfg->last_selected_section = sel.section;
    layer_mark_dirty(menu_layer_get_layer(cfg->menu_layer));
  }
}

static void prv_menu_nav_up_handler(ClickRecognizerRef recognizer, void *context) {
  MenuNavConfig *cfg = (MenuNavConfig*)context;
  if (!cfg || !cfg->menu_layer || !cfg->get_num_rows) return;
  uint16_t rows = cfg->get_num_rows();
  if (rows <= 1) return;

  MenuIndex sel = menu_layer_get_selected_index(cfg->menu_layer);
  if (sel.row == 0) {
    MenuIndex last = { .section = 0, .row = rows - 1 };
    menu_layer_set_selected_index(cfg->menu_layer, last, MenuRowAlignCenter, true);
  } else {
    menu_layer_set_selected_next(cfg->menu_layer, true, MenuRowAlignCenter, true);
  }
  prv_menu_nav_reset_marquee(cfg);
}

static void prv_menu_nav_down_handler(ClickRecognizerRef recognizer, void *context) {
  MenuNavConfig *cfg = (MenuNavConfig*)context;
  if (!cfg || !cfg->menu_layer || !cfg->get_num_rows) return;
  uint16_t rows = cfg->get_num_rows();
  if (rows <= 1) return;

  MenuIndex sel = menu_layer_get_selected_index(cfg->menu_layer);
  if (sel.row >= rows - 1) {
    MenuIndex first = { .section = 0, .row = 0 };
    menu_layer_set_selected_index(cfg->menu_layer, first, MenuRowAlignCenter, true);
  } else {
    menu_layer_set_selected_next(cfg->menu_layer, false, MenuRowAlignCenter, true);
  }
  prv_menu_nav_reset_marquee(cfg);
}

static void prv_menu_nav_provider(void *context) {
  MenuNavConfig *cfg = (MenuNavConfig*)context;
  if (cfg && cfg->original_provider) {
    cfg->original_provider(cfg->menu_layer);
  }
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_menu_nav_up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_menu_nav_down_handler);
}

void ui_menu_unbind_navigation(MenuNavConfig *cfg) {
  if (!cfg) return;
  if (cfg->marquee_timer) {
    app_timer_cancel(cfg->marquee_timer);
    cfg->marquee_timer = NULL;
  }
  if (s_active_nav_cfg == cfg) {
    s_active_nav_cfg = NULL;
  }
  cfg->window = NULL;
  cfg->menu_layer = NULL;
}

void ui_menu_bind_navigation(Window *window, MenuLayer *menu_layer, MenuNavConfig *cfg, uint16_t (*get_num_rows)(void)) {
  menu_layer_set_click_config_onto_window(menu_layer, window);
  cfg->window = window;
  cfg->menu_layer = menu_layer;
  cfg->get_num_rows = get_num_rows;
  cfg->original_provider = window_get_click_config_provider(window);
  window_set_click_config_provider_with_context(window, prv_menu_nav_provider, cfg);

  prv_menu_nav_reset_marquee(cfg);
  s_active_nav_cfg = cfg;

  if (cfg->marquee_timer) {
    app_timer_cancel(cfg->marquee_timer);
  }
  cfg->marquee_timer = app_timer_register(MARQUEE_TICK_MS, prv_marquee_timer_callback, cfg);
}

// =============================================================
// 1. Prestige Confirmation Sub-Menu
// =============================================================
static uint16_t prv_prestige_confirm_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 2;
}

static void prv_prestige_confirm_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  char pot_str[24];
  uint32_t payout = game_state_get_prestige_payout();
  game_state_format_currency(payout, pot_str, sizeof(pot_str));

  if (cell_index->row == 0) {
    char sub_buf[64];
    snprintf(sub_buf, sizeof(sub_buf), "+%s & +5%% Spd | Keeps Crowns", pot_str);
    ui_menu_cell_basic_draw(ctx, cell_layer, "> Confirm Prestige", sub_buf, NULL);
  } else {
    ui_menu_cell_basic_draw(ctx, cell_layer, "< Cancel", "Keep current colours", NULL);
  }
}

static void prv_prestige_confirm_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row == 0) {
    if (game_state_can_prestige()) {
      game_state_prestige();
      game_state_vibe_double();
      idle_pixel_pop_to_root();
    }
  } else {
    window_stack_pop(true);
  }
}

static uint16_t prv_prestige_confirm_get_row_count(void) {
  return 2;
}

static void prv_prestige_confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_prestige_confirm_menu = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_prestige_confirm_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_prestige_confirm_get_num_rows,
    .draw_row = prv_prestige_confirm_draw_row,
    .select_click = prv_prestige_confirm_select_callback,
  });
  ui_menu_bind_navigation(window, s_prestige_confirm_menu, &s_prestige_confirm_nav_cfg, prv_prestige_confirm_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_prestige_confirm_menu));
}

static void prv_prestige_confirm_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_prestige_confirm_nav_cfg);
  menu_layer_destroy(s_prestige_confirm_menu);
  s_prestige_confirm_menu = NULL;
  window_destroy(s_prestige_confirm_window);
  s_prestige_confirm_window = NULL;
}

static void prv_show_prestige_confirm(void) {
  if (s_prestige_confirm_window) return;
  s_prestige_confirm_window = window_create();
  window_set_window_handlers(s_prestige_confirm_window, (WindowHandlers) {
    .load = prv_prestige_confirm_window_load,
    .unload = prv_prestige_confirm_window_unload,
  });
  window_stack_push(s_prestige_confirm_window, true);
}

// =============================================================
// 2. Store Submenu (Upgrades & Boosts)
// =============================================================
static void prv_store_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "STORE",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 2, 50, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  char bal_buf[40];
  char cur_str[20];
  game_state_format_currency(game->data.currency, cur_str, sizeof(cur_str));
  snprintf(bal_buf, sizeof(bal_buf), "%s | %lu CRW", cur_str, (unsigned long)game_state_get_available_crowns());
  graphics_draw_text(ctx, bal_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(56, 2, bounds.size.w - 62, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_store_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return UPGRADE_COUNT;
}

static void prv_store_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  UpgradeId id = (UpgradeId)cell_index->row;
  const char *title = game_state_get_upgrade_title(id);
  char sub_buf[64];
  game_state_format_upgrade_sub(id, sub_buf, sizeof(sub_buf));
  ui_menu_cell_basic_draw(ctx, cell_layer, title, sub_buf, NULL);
}

static void prv_store_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  UpgradeId id = (UpgradeId)cell_index->row;
  bool bought = game_state_buy_upgrade(id);
  if (bought) {
    game_state_vibe_short();
    menu_layer_reload_data(menu_layer);
    if (s_store_header_layer) {
      layer_mark_dirty(s_store_header_layer);
    }
  } else {
    game_state_weak_vibe();
  }
}

static uint16_t prv_store_get_row_count(void) {
  return UPGRADE_COUNT;
}

static void prv_store_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_store_header_layer = layer_create(GRect(0, 0, bounds.size.w, 24));
  layer_set_update_proc(s_store_header_layer, prv_store_header_update_proc);
  layer_add_child(window_layer, s_store_header_layer);

  s_store_menu_layer = menu_layer_create(GRect(0, 24, bounds.size.w, bounds.size.h - 24));
  menu_layer_set_callbacks(s_store_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_store_get_num_rows,
    .draw_row = prv_store_draw_row,
    .select_click = prv_store_select_callback,
  });
  ui_menu_bind_navigation(window, s_store_menu_layer, &s_store_menu_nav_cfg, prv_store_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_store_menu_layer));
}

static void prv_store_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_store_menu_nav_cfg);
  menu_layer_destroy(s_store_menu_layer);
  s_store_menu_layer = NULL;
  layer_destroy(s_store_header_layer);
  s_store_header_layer = NULL;
  window_destroy(s_store_window);
  s_store_window = NULL;
}

void ui_store_show(void) {
  if (s_store_window) return;
  s_store_window = window_create();
  window_set_window_handlers(s_store_window, (WindowHandlers) {
    .load = prv_store_window_load,
    .unload = prv_store_window_unload,
  });
  window_stack_push(s_store_window, true);
}

// =============================================================
// 3. Reset Confirmation Dialog
// =============================================================
static void prv_reset_content_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GRect card_rect = GRect(6, 6, bounds.size.w - 12, bounds.size.h - 12);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_rect(ctx, card_rect);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "RESET GAME",
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 10, bounds.size.w, 28),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "Warning:\nThis deletes ALL progress, currency, saves & unlocks!\n\n> Confirm Reset\n[BACK] Cancel",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(12, 42, bounds.size.w - 24, bounds.size.h - 50),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_reset_select_handler(ClickRecognizerRef recognizer, void *context) {
  game_state_hard_reset();
  game_state_vibe_long();
  idle_pixel_pop_to_root();
}

static void prv_reset_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_reset_select_handler);
}

static void prv_reset_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_reset_content_layer = layer_create(bounds);
  layer_set_update_proc(s_reset_content_layer, prv_reset_content_update_proc);
  layer_add_child(window_layer, s_reset_content_layer);
}

static void prv_reset_window_unload(Window *window) {
  layer_destroy(s_reset_content_layer);
  s_reset_content_layer = NULL;
  window_destroy(s_reset_window);
  s_reset_window = NULL;
}

static void prv_show_reset_confirmation(void) {
  if (s_reset_window) return;
  s_reset_window = window_create();
  window_set_click_config_provider(s_reset_window, prv_reset_click_config_provider);
  window_set_window_handlers(s_reset_window, (WindowHandlers) {
    .load = prv_reset_window_load,
    .unload = prv_reset_window_unload,
  });
  window_stack_push(s_reset_window, true);
}

// =============================================================
// 4. Options Submenu
// =============================================================
static uint16_t prv_options_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 4;
}

static void prv_options_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Time Format",
                              game->data.option_24h ? "24-Hour" : "12-Hour", NULL);
      break;
    case 1:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Seconds",
                              game->data.option_show_seconds ? "ON" : "OFF", NULL);
      break;
    case 2:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Vibration",
                              game->data.option_vibration ? "ON" : "OFF", NULL);
      break;
    case 3:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Reset Game", "Wipe all data & restart", NULL);
      break;
    default:
      break;
  }
}

static void prv_options_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      game->data.option_24h = !game->data.option_24h;
      game_state_save();
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      break;
    case 1:
      game->data.option_show_seconds = !game->data.option_show_seconds;
      game_state_save();
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      break;
    case 2:
      game->data.option_vibration = !game->data.option_vibration;
      game_state_save();
      if (game->data.option_vibration) {
        vibes_short_pulse();
      }
      menu_layer_reload_data(menu_layer);
      break;
    case 3:
      prv_show_reset_confirmation();
      break;
    default:
      break;
  }
}

static uint16_t prv_options_get_row_count(void) {
  return 4;
}

static void prv_options_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_options_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_options_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_options_get_num_rows,
    .draw_row = prv_options_draw_row,
    .select_click = prv_options_select_callback,
  });
  ui_menu_bind_navigation(window, s_options_menu_layer, &s_options_menu_nav_cfg, prv_options_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_options_menu_layer));
}

static void prv_options_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_options_menu_nav_cfg);
  menu_layer_destroy(s_options_menu_layer);
  s_options_menu_layer = NULL;
  window_destroy(s_options_window);
  s_options_window = NULL;
}

void ui_options_show(void) {
  if (s_options_window) return;
  s_options_window = window_create();
  window_set_window_handlers(s_options_window, (WindowHandlers) {
    .load = prv_options_window_load,
    .unload = prv_options_window_unload,
  });
  window_stack_push(s_options_window, true);
}

// =============================================================
// 5. Main Navigation Menu
// =============================================================
static uint16_t prv_main_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 6;
}

static void prv_main_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0: { // Prestige with basic info (colours filled & money to earn)
      uint32_t solid_grids = game->data.solid_grids_count;
      char sub_buf[64];
      if (solid_grids == 0) {
        snprintf(sub_buf, sizeof(sub_buf), "0 Colours | Need >= 1");
      } else {
        char pot_str[24];
        uint32_t payout = game_state_get_prestige_payout();
        game_state_format_currency(payout, pot_str, sizeof(pot_str));
        snprintf(sub_buf, sizeof(sub_buf), "%lu Colours | +%s (+5%% Spd)",
                 (unsigned long)solid_grids, pot_str);
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Prestige", sub_buf, NULL);
      break;
    }
    case 1:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Store", "Upgrades & boosts", NULL);
      break;
    case 2:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Unlocks", "Colors & custom items", NULL);
      break;
    case 3:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Clock", "Open clock face", NULL);
      break;
    case 4:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Options", "Display & vibration", NULL);
      break;
    case 5:
      ui_menu_cell_basic_draw(ctx, cell_layer, "Save Slots", "Manage saves", NULL);
      break;
    default:
      break;
  }
}

static void prv_main_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0: // Prestige
      if (game_state_can_prestige()) {
        prv_show_prestige_confirm();
      } else {
        game_state_weak_vibe();
      }
      break;
    case 1:
      ui_store_show();
      break;
    case 2:
      ui_unlocks_show();
      break;
    case 3:
      ui_clock_show();
      break;
    case 4:
      ui_options_show();
      break;
    case 5:
      ui_save_slots_show();
      break;
    default:
      break;
  }
}

static uint16_t prv_main_menu_get_row_count(void) {
  return 6;
}

static void prv_main_menu_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_main_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_main_menu_get_num_rows,
    .draw_row = prv_main_menu_draw_row,
    .select_click = prv_main_menu_select_callback,
  });

  ui_menu_bind_navigation(window, s_main_menu_layer, &s_main_menu_nav_cfg, prv_main_menu_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_main_menu_layer));
}

static void prv_main_menu_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_main_menu_nav_cfg);
  menu_layer_destroy(s_main_menu_layer);
  s_main_menu_layer = NULL;
  window_destroy(s_main_menu_window);
  s_main_menu_window = NULL;
}

void ui_menu_show(void) {
  if (s_main_menu_window) return;
  s_main_menu_window = window_create();
  window_set_window_handlers(s_main_menu_window, (WindowHandlers) {
    .load = prv_main_menu_window_load,
    .unload = prv_main_menu_window_unload,
  });
  window_stack_push(s_main_menu_window, true);
}
