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
                     GRect(6, 2, 50, 20),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentLeft,
                     NULL);

  // Right Balance (Cash + Crown Counter)
  char bal_buf[40];
  char cur_str[20];
  game_state_format_currency(game->data.currency, cur_str, sizeof(cur_str));
  snprintf(bal_buf, sizeof(bal_buf), "%s | %lu CRW", cur_str, (unsigned long)game_state_get_available_crowns());
  graphics_draw_text(ctx, bal_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(56, 2, bounds.size.w - 62, 20),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight,
                     NULL);

  // Bottom border line
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, bounds.size.h - 1), GPoint(bounds.size.w, bounds.size.h - 1));
}

static uint16_t prv_menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return UPGRADE_COUNT;
}

static void prv_menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row >= UPGRADE_COUNT) return;
  UpgradeId id = (UpgradeId)cell_index->row;
  char sub_buf[96];
  game_state_format_upgrade_sub(id, sub_buf, sizeof(sub_buf));
  menu_cell_basic_draw(ctx, cell_layer, game_state_get_upgrade_title(id), sub_buf, NULL);
}

static void prv_menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  if (cell_index->row >= UPGRADE_COUNT) return;
  UpgradeId id = (UpgradeId)cell_index->row;

  if (game_state_buy_upgrade(id)) {
    game_state_weak_vibe();
  }

  layer_mark_dirty(s_header_layer);
  menu_layer_reload_data(menu_layer);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int header_h = 24;

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
