#include "ui_menu.h"
#include "game_state.h"
#include "ui_prestige.h"
#include "ui_store.h"
#include "ui_unlocks.h"
#include "ui_options.h"
#include "ui_save_slots.h"
#include "ui_clock.h"
#if ENABLE_DEV_MENU
#include "ui_dev_menu.h"
#endif

static Window *s_main_menu_window = NULL;
static MenuLayer *s_menu_layer = NULL;

static uint16_t prv_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
#if ENABLE_DEV_MENU
  return 7;
#else
  return 6;
#endif
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      menu_cell_basic_draw(ctx, cell_layer, "Prestige", "Reset & earn cents", NULL);
      break;
    case 1:
      menu_cell_basic_draw(ctx, cell_layer, "Store", "Upgrades & boosts", NULL);
      break;
    case 2:
      menu_cell_basic_draw(ctx, cell_layer, "Unlocks", "Colors & custom items", NULL);
      break;
    case 3:
      menu_cell_basic_draw(ctx, cell_layer, "Clock", "Open clock face", NULL);
      break;
    case 4:
      menu_cell_basic_draw(ctx, cell_layer, "Options", "Display & vibration", NULL);
      break;
    case 5:
      menu_cell_basic_draw(ctx, cell_layer, "Save Slots", "Manage 3 save files", NULL);
      break;
#if ENABLE_DEV_MENU
    case 6:
      menu_cell_basic_draw(ctx, cell_layer, "Dev Menu", "Cents & points ticker", NULL);
      break;
#endif
    default:
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  switch (cell_index->row) {
    case 0:
      ui_prestige_show();
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
#if ENABLE_DEV_MENU
    case 6:
      ui_dev_menu_show();
      break;
#endif
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
  window_destroy(s_main_menu_window);
  s_main_menu_window = NULL;
}

void ui_menu_show(void) {
  if (s_main_menu_window) {
    return;
  }
  s_main_menu_window = window_create();
  window_set_window_handlers(s_main_menu_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_main_menu_window, true);
}
