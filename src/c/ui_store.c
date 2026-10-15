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
                     GRect(8, 2, 60, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  // Right Balance (Persistent Pin formatted in cents / dollars)
  char bal_buf[32];
  char cur_str[24];
  game_state_format_currency(game->data.currency, cur_str, sizeof(cur_str));
  snprintf(bal_buf, sizeof(bal_buf), "Bal: %s", cur_str);
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
  return 9;
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  GameState *game = game_state_get();
  char sub_buf[96];
  char cost_str[24];

  switch (cell_index->row) {
    case 0: { // Auto-Fill Speed
      uint32_t interval = game_state_get_base_interval_ms();
      uint32_t cost = game_state_get_fill_speed_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu.%lus) | %s",
               game->data.upgrade_fill_speed_lvl,
               (unsigned long)(interval / 1000), (unsigned long)((interval % 1000) / 100),
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Auto-Fill Speed (+10%)", sub_buf, NULL);
      break;
    }
    case 1: { // Boost Amt
      uint32_t boost_amt = game_state_get_boost_amount_percent();
      uint32_t cost = game_state_get_boost_amount_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (+%lu%%) | %s",
               game->data.upgrade_boost_amount_lvl, (unsigned long)boost_amt,
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Amt (+5%)", sub_buf, NULL);
      break;
    }
    case 2: { // Boost Acc
      uint32_t accel = game_state_get_boost_accel_percent_per_sec();
      uint32_t cost = game_state_get_boost_accel_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (+%lu%%/s) | %s",
               game->data.upgrade_boost_accel_lvl, (unsigned long)accel,
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Acc (+10%/s)", sub_buf, NULL);
      break;
    }
    case 3: { // Boost Sustain
      uint32_t sustain_ms = game_state_get_boost_sustain_ms();
      uint32_t cost = game_state_get_boost_sustain_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu.%lus) | %s",
               game->data.upgrade_boost_sustain_lvl,
               (unsigned long)(sustain_ms / 1000), (unsigned long)((sustain_ms % 1000) / 100),
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Sustain (+0.5s)", sub_buf, NULL);
      break;
    }
    case 4: { // Boost Diminish Rate
      uint32_t decay_tenths = game_state_get_boost_diminish_tenths_per_sec();
      uint32_t cost = game_state_get_boost_diminish_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (-%lu.%lu%%/s) | %s",
               game->data.upgrade_boost_diminish_lvl,
               (unsigned long)(decay_tenths / 10), (unsigned long)(decay_tenths % 10),
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Diminish (-10%)", sub_buf, NULL);
      break;
    }
    case 5: { // Boost Max Limit
      uint32_t limit = game_state_get_boost_max_limit_percent();
      uint32_t cost = game_state_get_boost_limit_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu%%) | %s",
               game->data.upgrade_boost_limit_lvl, (unsigned long)limit,
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Boost Max Limit (+25%)", sub_buf, NULL);
      break;
    }
    case 6: { // Hold Auto-Press Rate
      uint32_t whole = game_state_get_hold_boost_clicks_whole();
      uint32_t tenths = game_state_get_hold_boost_clicks_tenths();
      uint32_t cost = game_state_get_hold_boost_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu.%lu clk/s) | %s",
               game->data.upgrade_hold_boost_lvl, (unsigned long)whole, (unsigned long)tenths,
               cost_str);
      menu_cell_basic_draw(ctx, cell_layer, "Hold Auto-Press (+10%)", sub_buf, NULL);
      break;
    }
    case 7: { // Max Idle Time (Offline Cap)
      uint32_t cost = game_state_get_offline_cap_cost();
      game_state_format_currency(cost, cost_str, sizeof(cost_str));
      if (!game_state_is_offline_unlocked()) {
        snprintf(sub_buf, sizeof(sub_buf), "Unlock 15m | %s", cost_str);
        menu_cell_basic_draw(ctx, cell_layer, "Max Idle Time (+15m)", sub_buf, NULL);
      } else {
        uint32_t off_min = game_state_get_max_offline_sec() / 60;
        snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lum) | %s",
                 game->data.upgrade_offline_cap_lvl, (unsigned long)off_min,
                 cost_str);
        menu_cell_basic_draw(ctx, cell_layer, "Max Idle Time (+15m)", sub_buf, NULL);
      }
      break;
    }
    case 8: { // Away Idle Speed (Efficiency)
      uint32_t eff_pct = game_state_get_offline_speed_percent();
      if (game->data.upgrade_offline_speed_lvl >= 9) {
        snprintf(sub_buf, sizeof(sub_buf), "Lvl 9 (100%%) | MAX");
      } else {
        uint32_t cost = game_state_get_offline_speed_cost();
        game_state_format_currency(cost, cost_str, sizeof(cost_str));
        snprintf(sub_buf, sizeof(sub_buf), "Lvl %d (%lu%%) | %s",
                 game->data.upgrade_offline_speed_lvl, (unsigned long)eff_pct,
                 cost_str);
      }
      menu_cell_basic_draw(ctx, cell_layer, "Away Idle Speed (+10%)", sub_buf, NULL);
      break;
    }
    default:
      break;
  }
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  bool bought = false;

  switch (cell_index->row) {
    case 0:
      bought = game_state_buy_fill_speed();
      break;
    case 1:
      bought = game_state_buy_boost_amount();
      break;
    case 2:
      bought = game_state_buy_boost_accel();
      break;
    case 3:
      bought = game_state_buy_boost_sustain();
      break;
    case 4:
      bought = game_state_buy_boost_diminish();
      break;
    case 5:
      bought = game_state_buy_boost_limit();
      break;
    case 6:
      bought = game_state_buy_hold_boost();
      break;
    case 7:
      bought = game_state_buy_offline_cap();
      break;
    case 8:
      bought = game_state_buy_offline_speed();
      break;
    default:
      break;
  }

  if (bought) {
    game_state_weak_vibe();
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
