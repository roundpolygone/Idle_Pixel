#include "ui_unlocks.h"
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
    snprintf(buf, sizeof(buf), "Preview: Color #%d", s_preview_color_idx);
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

static void prv_color_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GRect bounds = layer_get_bounds(cell_layer);
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  bool is_highlighted = menu_cell_layer_is_highlighted(cell_layer);
  GColor text_color = is_highlighted ? GColorWhite : GColorBlack;

  if (can_buy && cell_index->row == 0) {
    // Purchase row
    char buy_buf[48];
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock Color #%d (1 Pt)", game->data.unlocked_bg_colors);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock Color #%d (Need 1 Pt)", game->data.unlocked_bg_colors);
    }
    menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
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

    graphics_context_set_text_color(ctx, text_color);
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

    graphics_context_set_stroke_color(ctx, is_highlighted ? GColorWhite : GColorBlack);
    graphics_draw_rect(ctx, swatch_rect);
    return;
  }

  uint8_t color_idx = (uint8_t)(cell_index->row - buy_offset - 1);
  char title[32];
  if (color_idx == game->data.selected_bg_color_index) {
    snprintf(title, sizeof(title), "Color #%d [Active]", color_idx);
  } else {
    snprintf(title, sizeof(title), "Color #%d", color_idx);
  }

  graphics_context_set_text_color(ctx, text_color);
  graphics_draw_text(ctx, title,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, 4, bounds.size.w - 48, 24),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  GColor swatch_color = game_state_get_pixel_color(color_idx);
  int swatch_size = 22;
  int swatch_x = bounds.size.w - swatch_size - 10;
  int swatch_y = (bounds.size.h - swatch_size) / 2;
  GRect swatch_rect = GRect(swatch_x, swatch_y, swatch_size, swatch_size);

  graphics_context_set_fill_color(ctx, swatch_color);
  graphics_fill_rect(ctx, swatch_rect, 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, is_highlighted ? GColorWhite : GColorBlack);
  graphics_draw_rect(ctx, swatch_rect);
}

static void prv_color_selection_changed(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  int buy_offset = can_buy ? 1 : 0;

  if (can_buy && new_index.row == 0) {
    s_preview_color_idx = game->data.unlocked_bg_colors;
  } else if (new_index.row == buy_offset) {
    s_preview_color_idx = BG_COLOR_AUTO;
  } else {
    s_preview_color_idx = (uint8_t)(new_index.row - buy_offset - 1);
  }

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
      if (game->data.option_vibration) vibes_short_pulse();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  if (cell_index->row == buy_offset) {
    game->data.selected_bg_color_index = BG_COLOR_AUTO;
    s_preview_color_idx = BG_COLOR_AUTO;
  } else {
    uint8_t color_idx = (uint8_t)(cell_index->row - buy_offset - 1);
    game->data.selected_bg_color_index = color_idx;
    s_preview_color_idx = color_idx;
  }

  game_state_save();

  if (game->data.option_vibration) {
    vibes_short_pulse();
  }

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
  });

  menu_layer_set_click_config_onto_window(s_color_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_color_menu_layer));

  bool can_buy = (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT);
  uint16_t initial_row = 0;
  if (game->data.selected_bg_color_index == BG_COLOR_AUTO) {
    initial_row = can_buy ? 1 : 0;
  } else {
    initial_row = (can_buy ? 1 : 0) + 1 + game->data.selected_bg_color_index;
  }
  menu_layer_set_selected_index(s_color_menu_layer, (MenuIndex){ .section = 0, .row = initial_row }, MenuRowAlignCenter, false);
}

static void prv_color_window_unload(Window *window) {
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
  bool can_buy = (game->data.unlocked_fonts < UNLOCK_FONTS_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_fonts;
}

static void prv_font_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts < UNLOCK_FONTS_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_font_name(game->data.unlocked_fonts);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t font_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_font_name(font_idx);
  const char *sub = (font_idx == game->data.selected_font) ? "[Active Font]" : NULL;
  menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_font_selection_changed(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fonts < UNLOCK_FONTS_COUNT);

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
  bool can_buy = (game->data.unlocked_fonts < UNLOCK_FONTS_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_font()) {
      if (game->data.option_vibration) vibes_short_pulse();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t font_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_font = font_idx;
  s_preview_font_idx = font_idx;
  game_state_save();

  if (game->data.option_vibration) vibes_short_pulse();
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

  menu_layer_set_click_config_onto_window(s_font_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_font_menu_layer));
}

static void prv_font_window_unload(Window *window) {
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
  bool can_buy = (game->data.unlocked_fill_orders < UNLOCK_FILL_ORDERS_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_fill_orders;
}

static void prv_fill_order_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fill_orders < UNLOCK_FILL_ORDERS_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_fill_order_name((FillOrderType)game->data.unlocked_fill_orders);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t order_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_fill_order_name((FillOrderType)order_idx);
  const char *sub = (order_idx == game->data.selected_fill_order) ? "[Active]" : NULL;
  menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_fill_order_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_fill_orders < UNLOCK_FILL_ORDERS_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_fill_order()) {
      if (game->data.option_vibration) vibes_short_pulse();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t order_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_fill_order = order_idx;
  game_state_save();

  if (game->data.option_vibration) vibes_short_pulse();
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

  menu_layer_set_click_config_onto_window(s_fill_order_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_fill_order_menu_layer));
}

static void prv_fill_order_window_unload(Window *window) {
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
  bool can_buy = (game->data.unlocked_clock_styles < UNLOCK_CLOCK_STYLES_COUNT);
  return (can_buy ? 1 : 0) + game->data.unlocked_clock_styles;
}

static void prv_clock_style_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_clock_styles < UNLOCK_CLOCK_STYLES_COUNT);

  if (can_buy && cell_index->row == 0) {
    char buy_buf[48];
    const char *next_name = game_state_get_clock_style_name(game->data.unlocked_clock_styles);
    if (game->data.unlock_points >= 1) {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (1 Pt)", next_name);
    } else {
      snprintf(buy_buf, sizeof(buy_buf), "+ Unlock %s (Need 1 Pt)", next_name);
    }
    menu_cell_basic_draw(ctx, cell_layer, buy_buf, "Tap to spend 1 unlock pt", NULL);
    return;
  }

  uint8_t style_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  const char *name = game_state_get_clock_style_name(style_idx);
  const char *sub = (style_idx == game->data.selected_clock_style) ? "[Active Style]" : NULL;
  menu_cell_basic_draw(ctx, cell_layer, name, sub, NULL);
}

static void prv_clock_style_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool can_buy = (game->data.unlocked_clock_styles < UNLOCK_CLOCK_STYLES_COUNT);

  if (can_buy && cell_index->row == 0) {
    if (game_state_buy_next_clock_style()) {
      if (game->data.option_vibration) vibes_short_pulse();
      menu_layer_reload_data(menu_layer);
      if (s_unlocks_header_layer) layer_mark_dirty(s_unlocks_header_layer);
    }
    return;
  }

  uint8_t style_idx = (uint8_t)(cell_index->row - (can_buy ? 1 : 0));
  game->data.selected_clock_style = style_idx;
  game_state_save();

  if (game->data.option_vibration) vibes_short_pulse();
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

  menu_layer_set_click_config_onto_window(s_clock_style_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_clock_style_menu_layer));
}

static void prv_clock_style_window_unload(Window *window) {
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
  snprintf(pts_buf, sizeof(pts_buf), "Points: %lu", (unsigned long)game->data.unlock_points);
  graphics_draw_text(ctx, pts_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - 85, 2, 77, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentRight,
                     NULL);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_unlocks_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 4;
}

static void prv_unlocks_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0: {
      char sub[48];
      if (game->data.unlocked_bg_colors < UNLOCK_BG_COLORS_COUNT) {
        snprintf(sub, sizeof(sub), "Next: #%d (1 Pt) | %d/64",
                 game->data.unlocked_bg_colors, game->data.unlocked_bg_colors);
      } else {
        snprintf(sub, sizeof(sub), "All 64 Unlocked");
      }
      menu_cell_basic_draw(ctx, cell_layer, "Background Colors", sub, NULL);
      break;
    }
    case 1: {
      char sub[48];
      if (game->data.unlocked_fill_orders < UNLOCK_FILL_ORDERS_COUNT) {
        const char *next_name = game_state_get_fill_order_name((FillOrderType)game->data.unlocked_fill_orders);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/6", next_name, game->data.unlocked_fill_orders);
      } else {
        snprintf(sub, sizeof(sub), "All 6 Unlocked");
      }
      menu_cell_basic_draw(ctx, cell_layer, "Fill Orders", sub, NULL);
      break;
    }
    case 2: {
      char sub[48];
      if (game->data.unlocked_fonts < UNLOCK_FONTS_COUNT) {
        const char *next_name = game_state_get_font_name(game->data.unlocked_fonts);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/4", next_name, game->data.unlocked_fonts);
      } else {
        snprintf(sub, sizeof(sub), "All 4 Unlocked");
      }
      menu_cell_basic_draw(ctx, cell_layer, "Fonts", sub, NULL);
      break;
    }
    case 3: {
      char sub[48];
      if (game->data.unlocked_clock_styles < UNLOCK_CLOCK_STYLES_COUNT) {
        const char *next_name = game_state_get_clock_style_name(game->data.unlocked_clock_styles);
        snprintf(sub, sizeof(sub), "Next: %s (1 Pt) | %d/4", next_name, game->data.unlocked_clock_styles);
      } else {
        snprintf(sub, sizeof(sub), "All 4 Unlocked");
      }
      menu_cell_basic_draw(ctx, cell_layer, "Clock Styles", sub, NULL);
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
#if defined(PBL_ROUND)
  header_h = 32;
#endif

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

  menu_layer_set_click_config_onto_window(s_unlocks_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_unlocks_menu_layer));
}

static void prv_unlocks_window_unload(Window *window) {
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
