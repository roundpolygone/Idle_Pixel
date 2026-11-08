#include "ui.h"
#include "game_state.h"
#include "Idle_Pixel.h"

static Window *s_slots_window = NULL;
static Layer *s_header_layer = NULL;
static MenuLayer *s_slots_menu_layer = NULL;

static Window *s_confirm_window = NULL;
static MenuLayer *s_confirm_menu_layer = NULL;

static MenuNavConfig s_slots_nav_cfg;
static MenuNavConfig s_confirm_nav_cfg;

static void prv_format_play_time(uint32_t dur_sec, char *buf, size_t buf_size) {
  uint32_t mins = dur_sec / 60;
  uint32_t hours = mins / 60;
  mins = mins % 60;
  if (hours > 0) {
    snprintf(buf, buf_size, "%luh %lum", (unsigned long)hours, (unsigned long)mins);
  } else {
    snprintf(buf, buf_size, "%lum %lus", (unsigned long)mins, (unsigned long)(dur_sec % 60));
  }
}

// -------------------------------------------------------------
// Header Layer: Archived Count & Usable (+Pending) Crowns
// -------------------------------------------------------------
static void prv_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Left: archived runs count
  char slots_buf[32];
  uint16_t archived_count = game_state_get_archived_count();
  snprintf(slots_buf, sizeof(slots_buf), "Archived: %u", (unsigned int)archived_count);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, slots_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 1, (bounds.size.w * 52) / 100 - 6, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);

  // Right: usable crowns (+ pending crowns in active save)
  char crowns_buf[32];
  uint32_t usable = game_state_get_available_crowns();
  uint32_t pending = game_state_get_active_save_crowns();
  if (pending > 0) {
    snprintf(crowns_buf, sizeof(crowns_buf), "\xF0\x9F\x91\x91 %lu (+%lu)",
             (unsigned long)usable, (unsigned long)pending);
  } else {
    snprintf(crowns_buf, sizeof(crowns_buf), "\xF0\x9F\x91\x91 %lu Stock",
             (unsigned long)usable);
  }
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, crowns_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect((bounds.size.w * 52) / 100, 1, (bounds.size.w * 48) / 100 - 6, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);

  // Bottom border line
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

// -------------------------------------------------------------
// New Save Confirmation Sub-Menu
// -------------------------------------------------------------
static uint16_t prv_confirm_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 2;
}

static void prv_confirm_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  uint32_t pending = game_state_get_active_save_crowns();

  if (cell_index->row == 0) {
    char sub_buf[64];
    if (pending == 1) {
      snprintf(sub_buf, sizeof(sub_buf), "Release +1 \xF0\x9F\x91\x91 to stock & archive");
    } else if (pending > 1) {
      snprintf(sub_buf, sizeof(sub_buf), "Release +%lu \xF0\x9F\x91\x91 to stock & archive", (unsigned long)pending);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "Archive current run (0 \xF0\x9F\x91\x91)");
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, "> Confirm New Save", sub_buf, NULL);
  } else {
    ui_menu_cell_basic_draw(ctx, cell_layer, "< Cancel", "Keep playing current save", NULL);
  }
}

static void prv_confirm_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row == 0) {
    game_state_archive_and_start_new_run();
    game_state_vibe_double();
    idle_pixel_pop_to_root();
  } else {
    window_stack_pop(true);
  }
}

static uint16_t prv_confirm_get_row_count(void) {
  return 2;
}

static void prv_confirm_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_confirm_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_confirm_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_confirm_get_num_rows,
    .draw_row = prv_confirm_draw_row,
    .select_click = prv_confirm_select_callback,
  });
  ui_menu_bind_navigation(window, s_confirm_menu_layer, &s_confirm_nav_cfg, prv_confirm_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_confirm_menu_layer));
}

static void prv_confirm_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_confirm_nav_cfg);
  menu_layer_destroy(s_confirm_menu_layer);
  s_confirm_menu_layer = NULL;
  window_destroy(s_confirm_window);
  s_confirm_window = NULL;
}

static void prv_show_new_save_confirm(void) {
  if (s_confirm_window) return;
  s_confirm_window = window_create();
  window_set_window_handlers(s_confirm_window, (WindowHandlers) {
    .load = prv_confirm_window_load,
    .unload = prv_confirm_window_unload,
  });
  window_stack_push(s_confirm_window, true);
}

// -------------------------------------------------------------
// Save Slots & Archived Runs List Menu
// Row 0: + Start New Save
// Row 1: Current Active Save
// Rows 2+: Condensed Archived Runs (Read-Only)
// -------------------------------------------------------------
static uint16_t prv_slots_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 2 + game_state_get_archived_count();
}

static int16_t prv_slots_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row < 2) {
    return 44;
  }
  return 26; // Condensed single-line height for archived saves
}

static void prv_slots_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row == 0) {
    // Top of the list: + Start New Save
    uint32_t pending = game_state_get_active_save_crowns();
    char sub_buf[64];
    if (pending > 0) {
      snprintf(sub_buf, sizeof(sub_buf), "Archive & release \xF0\x9F\x91\x91 %lu to stock", (unsigned long)pending);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "Archive current & start fresh");
    }
    ui_menu_cell_basic_draw(ctx, cell_layer, "+ Start New Save", sub_buf, NULL);
    return;
  }

  if (cell_index->row == 1) {
    // Current Active Run
    GameState *game = game_state_get();
    uint32_t total_sec = game->data.active_play_sec + game->data.offline_play_sec;
    char time_str[24];
    prv_format_play_time(total_sec, time_str, sizeof(time_str));

    char title_buf[40];
    snprintf(title_buf, sizeof(title_buf), "Run #%u [Active]",
             (unsigned int)game_state_get_current_run_number());

    char sub_buf[64];
    snprintf(sub_buf, sizeof(sub_buf), "%s | \xF0\x9F\x91\x91 %lu Pending",
             time_str, (unsigned long)game->data.crowns);

    ui_menu_cell_basic_draw(ctx, cell_layer, title_buf, sub_buf, NULL);
    return;
  }

  // Condensed Archived Run Row (Read-Only)
  uint16_t archive_idx = (uint16_t)(cell_index->row - 2);
  ArchivedSaveEntry entry;
  if (!game_state_get_archived_save(archive_idx, &entry)) return;

  GRect bounds = layer_get_bounds(cell_layer);
  bool highlighted = menu_cell_layer_is_highlighted(cell_layer);

  // Fill background when highlighted
  graphics_context_set_fill_color(ctx, highlighted ? GColorWhite : GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GColor text_col = highlighted ? GColorBlack : GColorLightGray;
  GColor crown_col = highlighted ? GColorBlack : PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite);

  char time_str[24];
  prv_format_play_time(entry.total_time_sec, time_str, sizeof(time_str));

  char left_buf[40];
  snprintf(left_buf, sizeof(left_buf), "#%u  %s",
           (unsigned int)entry.run_number, time_str);

  char right_buf[24];
  snprintf(right_buf, sizeof(right_buf), "\xF0\x9F\x91\x91 %u", (unsigned int)entry.crowns);

  int margin = PBL_IF_ROUND_ELSE(18, 6);
  graphics_context_set_text_color(ctx, text_col);
  graphics_draw_text(ctx, left_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(margin, 1, bounds.size.w - (margin * 2) - 48, bounds.size.h - 2),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);

  graphics_context_set_text_color(ctx, crown_col);
  graphics_draw_text(ctx, right_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - margin - 54, 1, 54, bounds.size.h - 2),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);
}

static void prv_slots_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  ui_menu_reset_inactivity_timer();
  if (cell_index->row == 0) {
    prv_show_new_save_confirm();
  } else {
    // Active run and archived runs are read-only in this list
    game_state_weak_vibe();
  }
}

static uint16_t prv_slots_get_row_count(void) {
  return 2 + game_state_get_archived_count();
}

static void prv_slots_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Top Header Layer (24px height)
  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, 24));
  layer_set_update_proc(s_header_layer, prv_header_update_proc);
  layer_add_child(window_layer, s_header_layer);

  // Slots Menu Layer below header
  s_slots_menu_layer = menu_layer_create(GRect(0, 24, bounds.size.w, bounds.size.h - 24));
  menu_layer_set_callbacks(s_slots_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_slots_get_num_rows,
    .get_cell_height = prv_slots_get_cell_height,
    .draw_row = prv_slots_draw_row,
    .select_click = prv_slots_select_callback,
  });
  ui_menu_bind_navigation(window, s_slots_menu_layer, &s_slots_nav_cfg, prv_slots_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_slots_menu_layer));
}

static void prv_slots_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_slots_nav_cfg);
  menu_layer_destroy(s_slots_menu_layer);
  s_slots_menu_layer = NULL;
  layer_destroy(s_header_layer);
  s_header_layer = NULL;
  window_destroy(s_slots_window);
  s_slots_window = NULL;
}

void ui_save_slots_show(void) {
  if (s_slots_window) return;
  s_slots_window = window_create();
  window_set_window_handlers(s_slots_window, (WindowHandlers) {
    .load = prv_slots_window_load,
    .unload = prv_slots_window_unload,
  });
  window_stack_push(s_slots_window, true);
}
