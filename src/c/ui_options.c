#include "ui_options.h"
#include "game_state.h"

static Window *s_options_window = NULL;
static MenuLayer *s_menu_layer = NULL;

static uint16_t prv_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 3;
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Clock Style",
                           game->data.option_analog_clock ? "Analog" : "Digital", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Time Format",
                           game->data.option_24h ? "24-Hour" : "12-Hour", NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Vibration",
                           game->data.option_vibration ? "ON" : "OFF", NULL);
      break;
    default:
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();

  switch (cell_index->row) {
    case 0:
      game->data.option_analog_clock = !game->data.option_analog_clock;
      break;
    case 1:
      game->data.option_24h = !game->data.option_24h;
      break;
    case 2:
      game->data.option_vibration = !game->data.option_vibration;
      if (game->data.option_vibration) {
        vibes_short_pulse();
      }
      break;
    default:
      break;
  }

  game_state_save();
  menu_layer_reload_data(menu_layer);
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
