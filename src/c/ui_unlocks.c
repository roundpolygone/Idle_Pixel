#include "ui.h"
#include "game_state.h"

// Main Unlocks Menu
static Window *s_unlocks_window = NULL;
static Layer *s_unlocks_header_layer = NULL;
static MenuLayer *s_unlocks_menu_layer = NULL;

// Sub-Menu Windows
static Window *s_color_window = NULL;
static Layer *s_color_preview_layer = NULL;
static MenuLayer *s_color_menu_layer = NULL;
static uint8_t s_preview_color_idx = 0;

static Window *s_font_window = NULL;
static Layer *s_font_preview_layer = NULL;
static MenuLayer *s_font_menu_layer = NULL;
static uint8_t s_preview_font_idx = 0;

static Window *s_fill_order_window = NULL;
static MenuLayer *s_fill_order_menu_layer = NULL;

static Window *s_clock_style_window = NULL;
static MenuLayer *s_clock_style_menu_layer = NULL;

static Window *s_font_size_window = NULL;
static MenuLayer *s_font_size_menu_layer = NULL;

static MenuNavConfig s_unlocks_nav_cfg;
static MenuNavConfig s_color_nav_cfg;
static MenuNavConfig s_font_nav_cfg;
static MenuNavConfig s_fill_order_nav_cfg;
static MenuNavConfig s_clock_style_nav_cfg;
static MenuNavConfig s_font_size_nav_cfg;

// =============================================================
// 1. Color Picker with Active Real-Time Background Preview
// =============================================================
static void prv_color_preview_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GColor color;
  char buf[36];

  if (s_preview_color_idx == BG_COLOR_AUTO) {
    color = game_state_get_bg_color();
    snprintf(buf, sizeof(buf), "Preview: Auto Contrast");
  } else {
    color = game_state_get_pixel_color(s_preview_color_idx);
    snprintf(buf, sizeof(buf), "Preview: %s", game_state_get_color_name(s_preview_color_idx));
  }

  // Draw active preview background in real-time
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(color);
  graphics_context_set_text_color(ctx, is_dark ? GColorWhite : GColorBlack);

  graphics_draw_text(ctx, buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(4, 4, bounds.size.w - 8, 22),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_stroke_color(ctx, is_dark ? GColorWhite : GColorBlack);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_color_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  return (can_buy ? 1 : 0) + 1 + game->data.unlocked_bg_colors;
}

static uint16_t prv_color_get_row_count(void) {
  return prv_color_get_num_rows(NULL, 0, NULL);
}

static void prv_update_bg_picker_colors(void) {
  if (!s_color_menu_layer) return;
  GColor color;
  if (s_preview_color_idx == BG_COLOR_AUTO) {
    color = game_state_get_bg_color();
  } else {
    color = game_state_get_pixel_color(s_preview_color_idx);
  }
  bool is_dark = game_state_is_color_dark(color);
  GColor fg_col = is_dark ? GColorWhite : GColorBlack;

  if (s_color_window) {
    window_set_background_color(s_color_window, color);
  }
  menu_layer_set_normal_colors(s_color_menu_layer, color, fg_col);
  menu_layer_set_highlight_colors(s_color_menu_layer, color, fg_col);
}

static void prv_color_draw_background(GContext *ctx, const Layer *bg_layer, bool highlight, void *context) {
  GColor color;
  if (s_preview_color_idx == BG_COLOR_AUTO) {
    color = game_state_get_bg_color();
  } else {
    color = game_state_get_pixel_color(s_preview_color_idx);
  }
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, layer_get_bounds(bg_layer), 0, GCornerNone);
}

static void prv_color_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GRect bounds = layer_get_bounds(cell_layer);
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);

  GColor active_bg;
  if (s_preview_color_idx == BG_COLOR_AUTO) {
    active_bg = game_state_get_bg_color();
  } else {
    active_bg = game_state_get_pixel_color(s_preview_color_idx);
  }
  bool is_dark = game_state_is_color_dark(active_bg);
  GColor contrast_fg = is_dark ? GColorWhite : GColorBlack;

  if (can_buy && cell_index->row == 0) {
    // Purchase row
    char buy_buf[48];
    const char *next_name = game_state_get_unlockable_bg_color_name(game->data.unlocked_bg_colors);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    if (is_highlighted) {
      GRect card_rect = GRect(4, 2, bounds.size.w - 8, bounds.size.h - 4);
      GColor card_bg = is_dark ? GColorBlack : GColorWhite;
      GColor card_fg = is_dark ? GColorWhite : GColorBlack;
      graphics_context_set_fill_color(ctx, card_bg);
      graphics_fill_rect(ctx, card_rect, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
      graphics_draw_round_rect(ctx, card_rect, 4);

      graphics_context_set_text_color(ctx, card_fg);
      ui_menu_draw_text_marquee(ctx, buy_buf,
                                fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                                GRect(8, 2, card_rect.size.w - 16, 20),
                                card_fg, true);
      graphics_draw_text(ctx, "Tap to spend 1 unlock pt",
                         fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(8, 22, card_rect.size.w - 16, 16),
                         GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentLeft,
                         NULL);
    } else {
      ui_menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    }
    return;
  }

  int buy_offset = can_buy ? 1 : 0;
  if (cell_index->row == buy_offset) {
    // Auto Contrast row
    char title[32];
    if (game->data.selected_bg_color_index == BG_COLOR_AUTO) {
      snprintf(title, sizeof(title), "Auto Contrast [Active]");
    } else {
      snprintf(title, sizeof(title), "Auto Contrast [Default]");
    }

    if (is_highlighted) {
      GRect card_rect = GRect(4, 2, bounds.size.w - 8, bounds.size.h - 4);
      GColor card_bg = is_dark ? GColorBlack : GColorWhite;
      GColor card_fg = is_dark ? GColorWhite : GColorBlack;
      graphics_context_set_fill_color(ctx, card_bg);
      graphics_fill_rect(ctx, card_rect, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
      graphics_draw_round_rect(ctx, card_rect, 4);

      graphics_context_set_text_color(ctx, card_fg);
      graphics_draw_text(ctx, title,
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(8, 4, card_rect.size.w - 48, 24),
                         GTextOverflowModeWordWrap,
                         GTextAlignmentLeft,
                         NULL);

      int swatch_size = 22;
      int swatch_x = card_rect.origin.x + card_rect.size.w - swatch_size - 8;
      int swatch_y = card_rect.origin.y + (card_rect.size.h - swatch_size) / 2;
      GRect swatch_rect = GRect(swatch_x, swatch_y, swatch_size, swatch_size);

      GRect swatch_left = GRect(swatch_x, swatch_y, swatch_size / 2, swatch_size);
      GRect swatch_right = GRect(swatch_x + swatch_size / 2, swatch_y, swatch_size - swatch_size / 2, swatch_size);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, swatch_left, 0, GCornerNone);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, swatch_right, 0, GCornerNone);

      graphics_context_set_stroke_color(ctx, card_fg);
      graphics_draw_rect(ctx, swatch_rect);
    } else {
      graphics_context_set_text_color(ctx, contrast_fg);
      graphics_draw_text(ctx, title,
                         fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(8, 4, bounds.size.w - 48, 24),
                         GTextOverflowModeWordWrap,
                         GTextAlignmentLeft,
                         NULL);

      int swatch_size = 22;
      int swatch_x = bounds.size.w - swatch_size - 10;
      int swatch_y = (bounds.size.h - swatch_size) / 2;
      GRect swatch_rect = GRect(swatch_x, swatch_y, swatch_size, swatch_size);

      GRect swatch_left = GRect(swatch_x, swatch_y, swatch_size / 2, swatch_size);
      GRect swatch_right = GRect(swatch_x + swatch_size / 2, swatch_y, swatch_size - swatch_size / 2, swatch_size);
      graphics_context_set_fill_color(ctx, GColorBlack);
      graphics_fill_rect(ctx, swatch_left, 0, GCornerNone);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, swatch_right, 0, GCornerNone);

      graphics_context_set_stroke_color(ctx, contrast_fg);
      graphics_draw_rect(ctx, swatch_rect);
    }
    return;
  }

  uint8_t list_idx = (uint8_t)(cell_index->row - buy_offset - 1);
  uint8_t pebble_color = game_state_get_unlockable_bg_color(list_idx);
  const char *color_name = game_state_get_unlockable_bg_color_name(list_idx);

  char title[32];
  if (pebble_color == game->data.selected_bg_color_index) {
    snprintf(title, sizeof(title), "%s [Active]", color_name);
  } else {
    snprintf(title, sizeof(title), "%s", color_name);
  }

  GColor swatch_color = game_state_get_pixel_color(pebble_color);
  int swatch_size = 22;

  if (is_highlighted) {
    GRect card_rect = GRect(4, 2, bounds.size.w - 8, bounds.size.h - 4);
    GColor card_bg = is_dark ? GColorBlack : GColorWhite;
    GColor card_fg = is_dark ? GColorWhite : GColorBlack;
    graphics_context_set_fill_color(ctx, card_bg);
    graphics_fill_rect(ctx, card_rect, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, is_dark ? GColorLightGray : GColorDarkGray);
    graphics_draw_round_rect(ctx, card_rect, 4);

    graphics_context_set_text_color(ctx, card_fg);
    graphics_draw_text(ctx, title,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, 4, card_rect.size.w - 48, 24),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);

    int swatch_x = card_rect.origin.x + card_rect.size.w - swatch_size - 8;
    int swatch_y = card_rect.origin.y + (card_rect.size.h - swatch_size) / 2;
    GRect swatch_rect = GRect(swatch_x, swatch_y, swatch_size, swatch_size);

    graphics_context_set_fill_color(ctx, swatch_color);
    graphics_fill_rect(ctx, swatch_rect, 2, GCornersAll);
    graphics_context_set_stroke_color(ctx, card_fg);
    graphics_draw_rect(ctx, swatch_rect);
  } else {
    graphics_context_set_text_color(ctx, contrast_fg);
    graphics_draw_text(ctx, title,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(8, 4, bounds.size.w - 48, 24),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentLeft,
                       NULL);

    int swatch_x = bounds.size.w - swatch_size - 10;
    int swatch_y = (bounds.size.h - swatch_size) / 2;
    GRect swatch_rect = GRect(swatch_x, swatch_y, swatch_size, swatch_size);

    graphics_context_set_fill_color(ctx, swatch_color);
    graphics_fill_rect(ctx, swatch_rect, 2, GCornersAll);
    graphics_context_set_stroke_color(ctx, contrast_fg);
    graphics_draw_rect(ctx, swatch_rect);
  }
}

static void prv_color_selection_changed(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  int buy_offset = can_buy ? 1 : 0;

  if (can_buy && new_index.row == 0) {
    s_preview_color_idx = game_state_get_unlockable_bg_color(game->data.unlocked_bg_colors);
  } else if (new_index.row == buy_offset) {
    s_preview_color_idx = BG_COLOR_AUTO;
  } else {
    uint8_t list_idx = (uint8_t)(new_index.row - buy_offset - 1);
    s_preview_color_idx = game_state_get_unlockable_bg_color(list_idx);
  }

  prv_update_bg_picker_colors();

  if (s_color_preview_layer) {
    layer_mark_dirty(s_color_preview_layer);
  }
}

static void prv_color_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  int buy_offset = can_buy ? 1 : 0;

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_bg_color()) {
      game_state_weak_vibe();
      if (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT) {
        s_preview_color_idx = game_state_get_unlockable_bg_color(game->data.unlocked_bg_colors);
      } else {
        s_preview_color_idx = game->data.selected_bg_color_index;
      }
      prv_update_bg_picker_colors();
      if (s_color_preview_layer) layer_mark_dirty(s_color_preview_layer);
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  if (cell_index->row == buy_offset) {
    game->data.selected_bg_color_index = BG_COLOR_AUTO;
    s_preview_color_idx = BG_COLOR_AUTO;
  } else {
    uint8_t list_idx = (uint8_t)(cell_index->row - buy_offset - 1);
    uint8_t pebble_color = game_state_get_unlockable_bg_color(list_idx);
    game->data.selected_bg_color_index = pebble_color;
    s_preview_color_idx = pebble_color;
  }

  game_state_save();
  game_state_weak_vibe();

  prv_update_bg_picker_colors();
  if (s_color_preview_layer) layer_mark_dirty(s_color_preview_layer);
  menu_layer_reload_data(menu_layer);
}

static void prv_color_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GameState *game = game_state_get();

  s_preview_color_idx = game->data.selected_bg_color_index;
  int preview_h = 28;

  s_color_preview_layer = layer_create(GRect(0, 0, bounds.size.w, preview_h));
  layer_set_update_proc(s_color_preview_layer, prv_color_preview_update_proc);
  layer_add_child(window_layer, s_color_preview_layer);

  GRect menu_bounds = GRect(0, preview_h, bounds.size.w, bounds.size.h - preview_h);
  s_color_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_color_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_color_get_num_rows,
    .draw_row = prv_color_draw_row,
    .select_click = prv_color_select_callback,
    .selection_changed = prv_color_selection_changed,
    .draw_background = prv_color_draw_background,
  });

  menu_layer_set_center_focused(s_color_menu_layer, true);
  ui_menu_bind_navigation(window, s_color_menu_layer, &s_color_nav_cfg, prv_color_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_color_menu_layer));

  prv_update_bg_picker_colors();

  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  uint16_t initial_row = 0;
  if (game->data.selected_bg_color_index == BG_COLOR_AUTO) {
    initial_row = can_buy ? 1 : 0;
  } else {
    for (uint8_t i = 0; i < game->data.unlocked_bg_colors; i++) {
      if (game_state_get_unlockable_bg_color(i) == game->data.selected_bg_color_index) {
        initial_row = (can_buy ? 1 : 0) + 1 + i;
        break;
      }
    }
  }
  menu_layer_set_selected_index(s_color_menu_layer, (MenuIndex){ .section = 0, .row = initial_row }, MenuRowAlignCenter, false);
}

static void prv_color_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_color_nav_cfg);
  layer_destroy(s_color_preview_layer);
  s_color_preview_layer = NULL;
  menu_layer_destroy(s_color_menu_layer);
  s_color_menu_layer = NULL;
  window_destroy(s_color_window);
  s_color_window = NULL;
}

static void prv_show_color_picker(void) {
  if (s_color_window) return;
  s_color_window = window_create();
  window_set_window_handlers(s_color_window, (WindowHandlers) {
    .load = prv_color_window_load,
    .unload = prv_color_window_unload,
  });
  window_stack_push(s_color_window, true);
}

// =============================================================
// 2. Font Picker with Active Real-Time Font Preview
// =============================================================
static void prv_font_preview_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  GFont font = game_state_get_font(s_preview_font_idx, true);
  graphics_draw_text(ctx, "12:45",
                     font,
                     GRect(0, 0, bounds.size.w, bounds.size.h - 1),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_font_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts <= UNLOCK_FONTS_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_fonts;
}

static uint16_t prv_font_get_row_count(void) {
  return prv_font_get_num_rows(NULL, 0, NULL);
}

static void prv_font_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts <= UNLOCK_FONTS_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_font_name(game->data.unlocked_fonts);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t font_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_font_name(font_idx);
  const char *sub = (font_idx == game->data.selected_font) ? "[Active Font]" : NULL;
  ui_menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_font_selection_changed(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts <= UNLOCK_FONTS_COUNT);

  if (can_buy && new_index.row == 0) {
    s_preview_font_idx = game->data.unlocked_fonts;
  } else {
    s_preview_font_idx = (uint8_t)(new_index.row - (can_buy ? 1 : 0));
  }

  if (s_font_preview_layer) {
    layer_mark_dirty(s_font_preview_layer);
  }
}

static void prv_font_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts <= UNLOCK_FONTS_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_font()) {
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t font_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_font = font_idx;
  s_preview_font_idx = font_idx;
  game_state_save();

  game_state_weak_vibe();
  if (s_font_preview_layer) layer_mark_dirty(s_font_preview_layer);
  menu_layer_reload_data(menu_layer);
}

static void prv_font_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  GameState *game = game_state_get();

  s_preview_font_idx = game->data.selected_font;
  int preview_h = 38;

  s_font_preview_layer = layer_create(GRect(0, 0, bounds.size.w, preview_h));
  layer_set_update_proc(s_font_preview_layer, prv_font_preview_update_proc);
  layer_add_child(window_layer, s_font_preview_layer);

  GRect menu_bounds = GRect(0, preview_h, bounds.size.w, bounds.size.h - preview_h);
  s_font_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_font_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_font_get_num_rows,
    .draw_row = prv_font_draw_row,
    .select_click = prv_font_select_callback,
    .selection_changed = prv_font_selection_changed,
  });

  ui_menu_bind_navigation(window, s_font_menu_layer, &s_font_nav_cfg, prv_font_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_font_menu_layer));
}

static void prv_font_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_font_nav_cfg);
  layer_destroy(s_font_preview_layer);
  s_font_preview_layer = NULL;
  menu_layer_destroy(s_font_menu_layer);
  s_font_menu_layer = NULL;
  window_destroy(s_font_window);
  s_font_window = NULL;
}

static void prv_show_font_picker(void) {
  if (s_font_window) return;
  s_font_window = window_create();
  window_set_window_handlers(s_font_window, (WindowHandlers) {
    .load = prv_font_window_load,
    .unload = prv_font_window_unload,
  });
  window_stack_push(s_font_window, true);
}

// =============================================================
// 3. Fill Order Sub-Menu
// =============================================================
static uint16_t prv_fill_order_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fill_orders <= UNLOCK_FILL_ORDERS_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_fill_orders;
}

static uint16_t prv_fill_order_get_row_count(void) {
  return prv_fill_order_get_num_rows(NULL, 0, NULL);
}

static void prv_fill_order_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fill_orders <= UNLOCK_FILL_ORDERS_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_fill_order_name((FillOrderType)game->data.unlocked_fill_orders);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t order_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_fill_order_name((FillOrderType)order_idx);
  const char *sub = (order_idx == game->data.selected_fill_order) ? "[Active]" : NULL;
  ui_menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_fill_order_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fill_orders <= UNLOCK_FILL_ORDERS_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_fill_order()) {
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t order_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_fill_order = order_idx;
  game_state_save();

  game_state_weak_vibe();
  menu_layer_reload_data(menu_layer);
}

static void prv_fill_order_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_fill_order_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_fill_order_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_fill_order_get_num_rows,
    .draw_row = prv_fill_order_draw_row,
    .select_click = prv_fill_order_select_callback,
  });

  ui_menu_bind_navigation(window, s_fill_order_menu_layer, &s_fill_order_nav_cfg, prv_fill_order_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_fill_order_menu_layer));
}

static void prv_fill_order_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_fill_order_nav_cfg);
  menu_layer_destroy(s_fill_order_menu_layer);
  s_fill_order_menu_layer = NULL;
  window_destroy(s_fill_order_window);
  s_fill_order_window = NULL;
}

static void prv_show_fill_order_picker(void) {
  if (s_fill_order_window) return;
  s_fill_order_window = window_create();
  window_set_window_handlers(s_fill_order_window, (WindowHandlers) {
    .load = prv_fill_order_window_load,
    .unload = prv_fill_order_window_unload,
  });
  window_stack_push(s_fill_order_window, true);
}

// =============================================================
// 4. Clock Style Sub-Menu
// =============================================================
static uint16_t prv_clock_style_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_clock_styles <= UNLOCK_CLOCK_STYLES_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_clock_styles;
}

static uint16_t prv_clock_style_get_row_count(void) {
  return prv_clock_style_get_num_rows(NULL, 0, NULL);
}

static void prv_clock_style_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_clock_styles <= UNLOCK_CLOCK_STYLES_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_clock_style_name(game->data.unlocked_clock_styles);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t style_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_clock_style_name(style_idx);
  const char *sub = (style_idx == game->data.selected_clock_style) ? "[Active Style]" : NULL;
  ui_menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_clock_style_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_clock_styles <= UNLOCK_CLOCK_STYLES_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_clock_style()) {
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t style_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_clock_style = style_idx;
  game_state_save();

  game_state_weak_vibe();
  menu_layer_reload_data(menu_layer);
}

static void prv_clock_style_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_clock_style_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_clock_style_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_clock_style_get_num_rows,
    .draw_row = prv_clock_style_draw_row,
    .select_click = prv_clock_style_select_callback,
  });

  ui_menu_bind_navigation(window, s_clock_style_menu_layer, &s_clock_style_nav_cfg, prv_clock_style_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_clock_style_menu_layer));
}

static void prv_clock_style_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_clock_style_nav_cfg);
  menu_layer_destroy(s_clock_style_menu_layer);
  s_clock_style_menu_layer = NULL;
  window_destroy(s_clock_style_window);
  s_clock_style_window = NULL;
}

static void prv_show_clock_style_picker(void) {
  if (s_clock_style_window) return;
  s_clock_style_window = window_create();
  window_set_window_handlers(s_clock_style_window, (WindowHandlers) {
    .load = prv_clock_style_window_load,
    .unload = prv_clock_style_window_unload,
  });
  window_stack_push(s_clock_style_window, true);
}

// =============================================================
// 5. Font Size Sub-Menu
// =============================================================
static uint16_t prv_font_size_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_font_sizes < UNLOCK_FONT_SIZES_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_font_sizes;
}

static uint16_t prv_font_size_get_row_count(void) {
  return prv_font_size_get_num_rows(NULL, 0, NULL);
}

static void prv_font_size_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_font_sizes < UNLOCK_FONT_SIZES_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_font_size_name(game->data.unlocked_font_sizes);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t size_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_font_size_name(size_idx);
  const char *sub = (size_idx == game->data.selected_font_size) ? "[Active Size]" : NULL;
  ui_menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_font_size_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_font_sizes < UNLOCK_FONT_SIZES_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_font_size()) {
      game_state_weak_vibe();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t size_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_font_size = size_idx;
  game_state_save();

  game_state_weak_vibe();
  menu_layer_reload_data(menu_layer);
}

static void prv_font_size_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_font_size_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_font_size_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_font_size_get_num_rows,
    .draw_row = prv_font_size_draw_row,
    .select_click = prv_font_size_select_callback,
  });

  ui_menu_bind_navigation(window, s_font_size_menu_layer, &s_font_size_nav_cfg, prv_font_size_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_font_size_menu_layer));
}

static void prv_font_size_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_font_size_nav_cfg);
  menu_layer_destroy(s_font_size_menu_layer);
  s_font_size_menu_layer = NULL;
  window_destroy(s_font_size_window);
  s_font_size_window = NULL;
}

static void prv_show_font_size_picker(void) {
  if (s_font_size_window) return;
  s_font_size_window = window_create();
  window_set_window_handlers(s_font_size_window, (WindowHandlers) {
    .load = prv_font_size_window_load,
    .unload = prv_font_size_window_unload,
  });
  window_stack_push(s_font_size_window, true);
}

// =============================================================
// Unlocks Main Menu with Persistent Pinned Points Header
// =============================================================
static void prv_unlocks_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "UNLOCKS",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, 2, 75, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  char pts_buf[32];
  snprintf(pts_buf, sizeof(pts_buf), "Pts: %lu | %lu/64",
           (unsigned long)game->data.unlock_points,
           (unsigned long)game_state_get_total_unlocks_count());
  graphics_draw_text(ctx, pts_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - 100, 2, 92, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentRight,
                     NULL);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_unlocks_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 5;
}

static uint16_t prv_unlocks_get_row_count(void) {
  return prv_unlocks_get_num_rows(NULL, 0, NULL);
}

static void prv_unlocks_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0: {
      char sub[48];
      if (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT) {
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/%d",
                 game_state_get_unlockable_bg_color_name(game->data.unlocked_bg_colors),
                 game->data.unlocked_bg_colors, UNLOCK_BG_COLORS_COUNT);
      } else {
        snprintf(sub, sizeof(sub), "All %d Unlocked", UNLOCK_BG_COLORS_COUNT);
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Background Colors", sub, NULL);
      break;
    }
    case 1: {
      char sub[48];
      if (game->data.unlocked_fill_orders <= UNLOCK_FILL_ORDERS_COUNT) {
        const char *next_name = game_state_get_fill_order_name((FillOrderType)game->data.unlocked_fill_orders);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/8", next_name, game->data.unlocked_fill_orders);
      } else {
        snprintf(sub, sizeof(sub), "All 8 Unlocked");
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Fill Orders", sub, NULL);
      break;
    }
    case 2: {
      char sub[48];
      if (game->data.unlocked_fonts <= UNLOCK_FONTS_COUNT) {
        const char *next_name = game_state_get_font_name(game->data.unlocked_fonts);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/4", next_name, game->data.unlocked_fonts);
      } else {
        snprintf(sub, sizeof(sub), "All 4 Unlocked");
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Fonts", sub, NULL);
      break;
    }
    case 3: {
      char sub[48];
      if (game->data.unlocked_font_sizes < UNLOCK_FONT_SIZES_COUNT) {
        const char *next_name = game_state_get_font_size_name(game->data.unlocked_font_sizes);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/3", next_name, game->data.unlocked_font_sizes);
      } else {
        snprintf(sub, sizeof(sub), "All 3 Unlocked");
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Font Sizes", sub, NULL);
      break;
    }
    case 4: {
      char sub[48];
      if (game->data.unlocked_clock_styles <= UNLOCK_CLOCK_STYLES_COUNT) {
        const char *next_name = game_state_get_clock_style_name(game->data.unlocked_clock_styles);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/6", next_name, game->data.unlocked_clock_styles);
      } else {
        snprintf(sub, sizeof(sub), "All 6 Unlocked");
      }
      ui_menu_cell_basic_draw(ctx, cell_layer, "Clock Styles", sub, NULL);
      break;
    }
    default:
      break;
  }
}

static void prv_unlocks_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      prv_show_color_picker();
      break;
    case 1:
      prv_show_fill_order_picker();
      break;
    case 2:
      prv_show_font_picker();
      break;
    case 3:
      prv_show_font_size_picker();
      break;
    case 4:
      prv_show_clock_style_picker();
      break;
    default:
      break;
  }
}

static void prv_unlocks_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int header_h = 24;

  s_unlocks_header_layer = layer_create(GRect(0, 0, bounds.size.w, header_h));
  layer_set_update_proc(s_unlocks_header_layer, prv_unlocks_header_update_proc);
  layer_add_child(window_layer, s_unlocks_header_layer);

  GRect menu_bounds = GRect(0, header_h, bounds.size.w, bounds.size.h - header_h);
  s_unlocks_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_unlocks_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_unlocks_get_num_rows,
    .draw_row = prv_unlocks_draw_row,
    .select_click = prv_unlocks_select_callback,
  });

  ui_menu_bind_navigation(window, s_unlocks_menu_layer, &s_unlocks_nav_cfg, prv_unlocks_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_unlocks_menu_layer));
}

static void prv_unlocks_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_unlocks_nav_cfg);
  layer_destroy(s_unlocks_header_layer);
  s_unlocks_header_layer = NULL;
  menu_layer_destroy(s_unlocks_menu_layer);
  s_unlocks_menu_layer = NULL;
  window_destroy(s_unlocks_window);
  s_unlocks_window = NULL;
}

void ui_unlocks_show(void) {
  if (s_unlocks_window) return;
  s_unlocks_window = window_create();
  window_set_window_handlers(s_unlocks_window, (WindowHandlers) {
    .load = prv_unlocks_window_load,
    .unload = prv_unlocks_window_unload,
  });
  window_stack_push(s_unlocks_window, true);
}
