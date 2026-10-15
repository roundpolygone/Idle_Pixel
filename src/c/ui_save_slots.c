#include "ui_save_slots.h"
#include "game_state.h"

static Window *s_slots_window = NULL;
static MenuLayer *s_slots_menu_layer = NULL;

static Window *s_action_window = NULL;
static MenuLayer *s_action_menu_layer = NULL;
static uint8_t s_selected_slot = 0;

// -------------------------------------------------------------
// Slot Actions Sub-Menu: Load, Save, Clear
// -------------------------------------------------------------
static uint16_t prv_action_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  SaveSlotInfo info;
  game_state_get_slot_info(s_selected_slot, &info);
  return info.is_used ? 3 : 1;
}

static void prv_action_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  SaveSlotInfo info;
  game_state_get_slot_info(s_selected_slot, &info);

  if (!info.is_used) {
    menu_cell_basic_draw(ctx, cell_layer, "Save Current Game", "Save progress into this slot", NULL);
    return;
  }

  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Load Slot", "Switch to this save", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Overwrite Slot", "Save current game over this slot", NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Clear Slot", "Delete this save file", NULL);
      break;
    default:
      break;
  }
}

static void prv_action_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  SaveSlotInfo info;
  game_state_get_slot_info(s_selected_slot, &info);

  if (!info.is_used) {
    game_state_save_to_slot(s_selected_slot);
    vibes_short_pulse();
    window_stack_pop(true);
    if (s_slots_menu_layer) menu_layer_reload_data(s_slots_menu_layer);
    return;
  }

  switch (cell_index->row) {
    case 0: // Load
      game_state_switch_to_slot(s_selected_slot);
      vibes_short_pulse();
      window_stack_pop(true); // Pop action window
      window_stack_pop(true); // Pop slots window back to options
      break;
    case 1: // Overwrite
      game_state_save_to_slot(s_selected_slot);
      vibes_short_pulse();
      window_stack_pop(true);
      if (s_slots_menu_layer) menu_layer_reload_data(s_slots_menu_layer);
      break;
    case 2: // Clear
      game_state_clear_slot(s_selected_slot);
      vibes_double_pulse();
      window_stack_pop(true);
      if (s_slots_menu_layer) menu_layer_reload_data(s_slots_menu_layer);
      break;
    default:
      break;
  }
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
  menu_layer_set_click_config_onto_window(s_action_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_action_menu_layer));
}

static void prv_action_window_unload(Window *window) {
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
  return NUM_SAVE_SLOTS;
}

static void prv_slots_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  uint8_t slot_idx = (uint8_t)cell_index->row;
  SaveSlotInfo info;
  game_state_get_slot_info(slot_idx, &info);

  char title_buf[48];
  char sub_buf[64];

  bool is_active = (slot_idx == game_state_get_active_slot());

  if (!info.is_used) {
    snprintf(title_buf, sizeof(title_buf), "Slot %u: [Empty]", (unsigned int)(slot_idx + 1));
    snprintf(sub_buf, sizeof(sub_buf), "Tap to save game here");
  } else {
    // Format date started
    struct tm *time_info = localtime(&info.date_started);
    char date_str[24];
    if (info.date_started > 0 && time_info) {
      strftime(date_str, sizeof(date_str), "%b %d", time_info);
    } else {
      snprintf(date_str, sizeof(date_str), "Started");
    }

    if (is_active) {
      snprintf(title_buf, sizeof(title_buf), "Slot %u: %s [Active]", (unsigned int)(slot_idx + 1), date_str);
    } else {
      snprintf(title_buf, sizeof(title_buf), "Slot %u: %s", (unsigned int)(slot_idx + 1), date_str);
    }

    // Format duration
    uint32_t dur_sec = info.is_completed ? info.completion_time_sec : info.total_time_sec;
    uint32_t mins = dur_sec / 60;
    uint32_t hours = mins / 60;
    mins = mins % 60;

    if (info.is_completed) {
      // Completed slot with crown symbol: 👑
      snprintf(sub_buf, sizeof(sub_buf), "\xF0\x9F\x91\x91 Completed in %luh %lum", (unsigned long)hours, (unsigned long)mins);
    } else {
      snprintf(sub_buf, sizeof(sub_buf), "Time: %luh %lum | %u/64 Colors",
               (unsigned long)hours, (unsigned long)mins, (unsigned int)info.tier2_pixels);
    }
  }

  menu_cell_basic_draw(ctx, cell_layer, title_buf, sub_buf, NULL);
}

static void prv_slots_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  prv_show_slot_action((uint8_t)cell_index->row);
}

static void prv_slots_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_slots_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_slots_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_slots_get_num_rows,
    .draw_row = prv_slots_draw_row,
    .select_click = prv_slots_select_callback,
  });
  menu_layer_set_click_config_onto_window(s_slots_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_slots_menu_layer));
}

static void prv_slots_window_unload(Window *window) {
  menu_layer_destroy(s_slots_menu_layer);
  s_slots_menu_layer = NULL;
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
