#include "ui.h"
#include "game_state.h"
#include "Idle_Pixel.h"

static Window *s_slots_window = NULL;
static Layer *s_header_layer = NULL;
static MenuLayer *s_slots_menu_layer = NULL;

static Window *s_action_window = NULL;
static MenuLayer *s_action_menu_layer = NULL;
static uint8_t s_selected_slot = 0;

static uint8_t s_used_slots[NUM_SAVE_SLOTS];
static uint16_t s_used_count = 0;

static MenuNavConfig s_slots_nav_cfg;
static MenuNavConfig s_action_nav_cfg;

static void prv_refresh_used_slots(void) {
  s_used_count = 0;
  for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
    SaveSlotInfo info;
    game_state_get_slot_info(i, &info);
    if (info.is_used) {
      s_used_slots[s_used_count++] = i;
    }
  }
}

// -------------------------------------------------------------
// Header Layer: Slots Used & Crowns Unspent/Earned
// -------------------------------------------------------------
static void prv_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Left: slots used
  char slots_buf[32];
  snprintf(slots_buf, sizeof(slots_buf), "Saves: %u/%d", s_used_count, NUM_SAVE_SLOTS);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, slots_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 1, (bounds.size.w * 55) / 100 - 6, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft,
                     NULL);

  // Right: crowns unspent/earned
  char crowns_buf[32];
  uint32_t unspent = game_state_get_available_crowns();
  uint32_t earned = game_state_get_total_crowns_earned();
  snprintf(crowns_buf, sizeof(crowns_buf), "\xF0\x9F\x91\x91 %lu/%lu", (unsigned long)unspent, (unsigned long)earned);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  graphics_draw_text(ctx, crowns_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect((bounds.size.w * 55) / 100, 1, (bounds.size.w * 45) / 100 - 6, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);

  // Bottom border line
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

// -------------------------------------------------------------
// Slot Actions Sub-Menu: Load, Overwrite, Clear
// -------------------------------------------------------------
static uint16_t prv_action_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 3;
}

static void prv_action_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      ui_menu_cell_basic_draw(ctx, cell_layer, "> Load Slot", "Switch to this save", NULL);
      break;
    case 1:
      ui_menu_cell_basic_draw(ctx, cell_layer, "> Overwrite Slot", "Save current game over this slot", NULL);
      break;
    case 2:
      ui_menu_cell_basic_draw(ctx, cell_layer, "> Clear Slot", "Delete this save file", NULL);
      break;
    default:
      break;
  }
}

static void prv_action_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0: // Load
      game_state_switch_to_slot(s_selected_slot);
      game_state_vibe_short();
      idle_pixel_pop_to_root();
      break;
    case 1: // Overwrite
      game_state_save_to_slot(s_selected_slot);
      game_state_vibe_short();
      window_stack_pop(true);
      prv_refresh_used_slots();
      if (s_slots_menu_layer) menu_layer_reload_data(s_slots_menu_layer);
      if (s_header_layer) layer_mark_dirty(s_header_layer);
      break;
    case 2: // Clear
      game_state_clear_slot(s_selected_slot);
      game_state_vibe_double();
      window_stack_pop(true);
      prv_refresh_used_slots();
      if (s_slots_menu_layer) {
        MenuIndex sel = menu_layer_get_selected_index(s_slots_menu_layer);
        uint16_t num_rows = s_used_count + (s_used_count < NUM_SAVE_SLOTS ? 1 : 0);
        if (sel.row >= num_rows && num_rows > 0) {
          sel.row = num_rows - 1;
          menu_layer_set_selected_index(s_slots_menu_layer, sel, MenuRowAlignCenter, false);
        }
        menu_layer_reload_data(s_slots_menu_layer);
      }
      if (s_header_layer) layer_mark_dirty(s_header_layer);
      break;
    default:
      break;
  }
}

static uint16_t prv_action_get_row_count(void) {
  return 3;
}

static void prv_action_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_action_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_action_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_action_get_num_rows,
    .draw_row = prv_action_draw_row,
    .select_click = prv_action_select_callback,
  });
  ui_menu_bind_navigation(window, s_action_menu_layer, &s_action_nav_cfg, prv_action_get_row_count);
  layer_add_child(window_layer, menu_layer_get_layer(s_action_menu_layer));
}

static void prv_action_window_unload(Window *window) {
  ui_menu_unbind_navigation(&s_action_nav_cfg);
  menu_layer_destroy(s_action_menu_layer);
  s_action_menu_layer = NULL;
  window_destroy(s_action_window);
  s_action_window = NULL;
}

static void prv_show_slot_action(uint8_t slot_idx) {
  s_selected_slot = slot_idx;
  if (s_action_window) return;
  s_action_window = window_create();
  window_set_window_handlers(s_action_window, (WindowHandlers) {
    .load = prv_action_window_load,
    .unload = prv_action_window_unload,
  });
  window_stack_push(s_action_window, true);
}

// -------------------------------------------------------------
// Slots List Menu
// -------------------------------------------------------------
static uint16_t prv_slots_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return s_used_count + (s_used_count < NUM_SAVE_SLOTS ? 1 : 0);
}

static void prv_slots_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row < s_used_count) {
    uint8_t slot_idx = s_used_slots[cell_index->row];
    SaveSlotInfo info;
    game_state_get_slot_info(slot_idx, &info);

    char title_buf[48];
    char sub_buf[64];

    bool is_active = (slot_idx == game_state_get_active_slot());

    // Format date started
    struct tm *time_info = localtime(&info.date_started);
    char date_str[24];
    if (info.date_started > 0 && time_info) {
      strftime(date_str, sizeof(date_str), "%b %d", time_info);
    } else {
      snprintf(date_str, sizeof(date_str), "Started");
    }

    if (is_active) {
      snprintf(title_buf, sizeof(title_buf), "Save %u: %s [Active]", (unsigned int)(slot_idx + 1), date_str);
    } else {
      snprintf(title_buf, sizeof(title_buf), "Save %u: %s", (unsigned int)(slot_idx + 1), date_str);
    }

    // Format duration & crown earned status
    uint32_t dur_sec = info.is_completed ? info.completion_time_sec : info.total_time_sec;
    uint32_t mins = dur_sec / 60;
    uint32_t hours = mins / 60;
    mins = mins % 60;

    if (info.is_completed) {
      snprintf(sub_buf, sizeof(sub_buf), "\xF0\x9F\x91\x91 Crown Earned | %luh %lum", (unsigned long)hours, (unsigned long)mins);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "No Crown | Time: %luh %lum", (unsigned long)hours, (unsigned long)mins);
    }

    ui_menu_cell_basic_draw(ctx, cell_layer, title_buf, sub_buf, NULL);
  } else {
    // Plus button to add a new save
    ui_menu_cell_basic_draw(ctx, cell_layer, "+ Add New Save", "Start a new run", NULL);
  }
}

static void prv_slots_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row < s_used_count) {
    uint8_t slot_idx = s_used_slots[cell_index->row];
    prv_show_slot_action(slot_idx);
  } else {
    // Find first unused slot
    uint8_t new_slot = 0;
    for (uint8_t i = 0; i < NUM_SAVE_SLOTS; i++) {
      SaveSlotInfo info;
      game_state_get_slot_info(i, &info);
      if (!info.is_used) {
        new_slot = i;
        break;
      }
    }
    game_state_start_new_run_in_slot(new_slot);
    game_state_vibe_short();
    idle_pixel_pop_to_root();
  }
}

static uint16_t prv_slots_get_row_count(void) {
  return s_used_count + (s_used_count < NUM_SAVE_SLOTS ? 1 : 0);
}

static void prv_slots_window_load(Window *window) {
  prv_refresh_used_slots();

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
