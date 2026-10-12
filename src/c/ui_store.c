#include "ui_store.h"
#include "game_state.h"

static Window *s_store_window = NULL;
static Layer *s_header_layer = NULL;
static MenuLayer *s_menu_layer = NULL;

static void prv_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Left Title
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "STORE",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, 2, 70, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  // Right Balance (Persistent Pin)
  char bal_buf[32];
  snprintf(bal_buf, sizeof(bal_buf), "Bal: %lu¢", (unsigned long)game->data.currency);
  graphics_draw_text(ctx, bal_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(bounds.size.w - 85, 2, 77, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentRight,
                     NULL);

  // Bottom border line
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return 4;
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  char sub_buf[48];

  switch (cell_index->row) {
    case 0: {
      uint32_t interval = game_state_get_base_interval_ms();
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu.%lus) | 1¢",
               game->data.upgrade_fill_speed_lvl,
               (unsigned long)(interval / 1000), (unsigned long)((interval % 1000) / 100));
      menu_cell_basic_draw(ctx, cell_layer, "Auto-Fill Speed", sub_buf, NULL);
      break;
    }
    case 1: {
      uint32_t boost_pct = game_state_get_boost_percent();
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (+%lu%% speed) | 1¢",
               game->data.upgrade_boost_power_lvl, (unsigned long)boost_pct);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Power", sub_buf, NULL);
      break;
    }
    case 2: {
      uint32_t len_ms = game_state_get_boost_duration_ms();
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu.%lus) | 1¢",
               game->data.upgrade_boost_len_lvl,
               (unsigned long)(len_ms / 1000), (unsigned long)((len_ms % 1000) / 100));
      menu_cell_basic_draw(ctx, cell_layer, "Boost Length", sub_buf, NULL);
      break;
    }
    case 3: {
      if (!game_state_is_offline_unlocked()) {
        menu_cell_basic_draw(ctx, cell_layer, "Max Idle Time", "Locked (Unlock 15m) | 1¢", NULL);
      } else {
        uint32_t off_min = game_state_get_max_offline_sec() / 60;
        snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lum cap) | 1¢",
                 game->data.upgrade_offline_cap_lvl, (unsigned long)off_min);
        menu_cell_basic_draw(ctx, cell_layer, "Max Idle Time", sub_buf, NULL);
      }
      break;
    }
    default:
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  bool bought = false;

  switch (cell_index->row) {
    case 0:
      bought = game_state_buy_fill_speed();
      break;
    case 1:
      bought = game_state_buy_boost_power();
      break;
    case 2:
      bought = game_state_buy_boost_len();
      break;
    case 3:
      bought = game_state_buy_offline_cap();
      break;
    default:
      break;
  }

  if (bought && game->data.option_vibration) {
    vibes_short_pulse();
  }

  layer_mark_dirty(s_header_layer);
  menu_layer_reload_data(menu_layer);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int header_h = 24;
#if defined(PBL_ROUND)
  header_h = 32;
#endif

  s_header_layer = layer_create(GRect(0, 0, bounds.size.w, header_h));
  layer_set_update_proc(s_header_layer, prv_header_update_proc);
  layer_add_child(window_layer, s_header_layer);

  GRect menu_bounds = GRect(0, header_h, bounds.size.w, bounds.size.h - header_h);
  s_menu_layer = menu_layer_create(menu_bounds);
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_menu_get_num_rows,
    .draw_row = prv_menu_draw_row,
    .select_click = prv_menu_select_callback,
  });

  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_header_layer);
  s_header_layer = NULL;
  menu_layer_destroy(s_menu_layer);
  s_menu_layer = NULL;
  window_destroy(s_store_window);
  s_store_window = NULL;
}

void ui_store_show(void) {
  if (s_store_window) {
    return;
  }
  s_store_window = window_create();
  window_set_window_handlers(s_store_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_store_window, true);
}
