#include "ui_options.h"
#include "game_state.h"
#include "ui_save_slots.h"

static Window *s_options_window = NULL;
static MenuLayer *s_menu_layer = NULL;

static Window *s_reset_window = NULL;
static Layer *s_reset_content_layer = NULL;

// -------------------------------------------------------------
// Reset Confirmation Window
// -------------------------------------------------------------
static void prv_reset_content_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Red/alert card border
  GRect card_rect = GRect(6, 6, bounds.size.w - 12, bounds.size.h - 12);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_rect(ctx, card_rect);

  // Title
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "RESET GAME",
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 10, bounds.size.w, 28),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Body
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "Warning:\nThis deletes ALL progress, currency, upgrades & unlocks!\n\n[SELECT] Confirm\n[BACK] Cancel",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(12, 42, bounds.size.w - 24, bounds.size.h - 50),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_reset_select_handler(ClickRecognizerRef recognizer, void *context) {
  game_state_hard_reset();
  vibes_long_pulse();

  // Close reset confirmation dialog and options menu back to root
  if (s_reset_window) {
    window_stack_pop(false);
  }
  if (s_options_window) {
    window_stack_pop(true);
  }
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
  if (s_reset_window) {
    return;
  }
  s_reset_window = window_create();
  window_set_click_config_provider(s_reset_window, prv_reset_click_config_provider);
  window_set_window_handlers(s_reset_window, (WindowHandlers) {
    .load = prv_reset_window_load,
    .unload = prv_reset_window_unload,
  });
  window_stack_push(s_reset_window, true);
}

// -------------------------------------------------------------
// Options Menu
// -------------------------------------------------------------
static uint16_t prv_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 5;
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Time Format",
                           game->data.option_24h ? "24-Hour" : "12-Hour", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Seconds",
                           game->data.option_show_seconds ? "ON" : "OFF", NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Vibration",
                           game->data.option_vibration ? "ON" : "OFF", NULL);
      break;
    case 3:
      menu_cell_basic_draw(ctx, cell_layer, "Save & Load Slots", "Manage enumerated saves", NULL);
      break;
    case 4:
      menu_cell_basic_draw(ctx, cell_layer, "Reset Game", "Wipe all data & restart", NULL);
      break;
    default:
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      game->data.option_24h = !game->data.option_24h;
      game_state_save();
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
      if (game->data.option_vibration) {
        game_state_weak_vibe();
      }
      game_state_save();
      menu_layer_reload_data(menu_layer);
      break;
    case 3:
      ui_save_slots_show();
      break;
    case 4:
      prv_show_reset_confirmation();
      break;
    default:
      break;
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_menu_layer = menu_layer_create(bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_menu_get_num_rows,
    .draw_row = prv_menu_draw_row,
    .select_click = prv_menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
  window_destroy(s_options_window);
  s_options_window = NULL;
}

void ui_options_show(void) {
  if (s_options_window) {
    return;
  }
  s_options_window = window_create();
  window_set_window_handlers(s_options_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_options_window, true);
}
