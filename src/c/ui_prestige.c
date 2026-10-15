#include "ui_prestige.h"
#include "game_state.h"

static Window *s_prestige_window = NULL;
static Layer *s_content_layer = NULL;

static void prv_content_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Title
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "PRESTIGE",
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 5, bounds.size.w, 28),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Info details
  char info_buf[140];
  uint32_t solid_grids = game->data.solid_grids_count;

  if (solid_grids == 0) {
    snprintf(info_buf, sizeof(info_buf),
             "Solid Grids: 0\nNeed >= 1 filled solid grid (1¢) to prestige.\n\n[BACK to return]");
    graphics_context_set_text_color(ctx, GColorLightGray);
  } else {
    char gain_str[24];
    game_state_format_currency(solid_grids, gain_str, sizeof(gain_str));
    snprintf(info_buf, sizeof(info_buf),
             "Solid Grids: %lu\nGain: %s\n\nResets solid grids!\n(Keeps Gold Pixels)\n\n[SELECT] Confirm\n[BACK] Cancel",
             (unsigned long)solid_grids, gain_str);
    graphics_context_set_text_color(ctx, GColorWhite);
  }

  graphics_draw_text(ctx, info_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(10, 36, bounds.size.w - 20, bounds.size.h - 40),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  GameState *game = game_state_get();
  if (game_state_can_prestige()) {
    game_state_prestige();
    if (game->data.option_vibration) {
      vibes_double_pulse();
    }
    window_stack_pop(true);
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_content_layer = layer_create(bounds);
  layer_set_update_proc(s_content_layer, prv_content_update_proc);
  layer_add_child(window_layer, s_content_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_content_layer);
  s_content_layer = NULL;
  window_destroy(s_prestige_window);
  s_prestige_window = NULL;
}

void ui_prestige_show(void) {
  if (s_prestige_window) {
    return;
  }
  s_prestige_window = window_create();
  window_set_click_config_provider(s_prestige_window, prv_click_config_provider);
  window_set_window_handlers(s_prestige_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_prestige_window, true);
}
