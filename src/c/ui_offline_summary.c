#include "ui_offline_summary.h"
#include "game_state.h"

static Window *s_summary_window = NULL;
static Layer *s_content_layer = NULL;

static void prv_format_duration(uint32_t seconds, char *buf, size_t buf_size) {
  uint32_t mins = seconds / 60;
  uint32_t hours = mins / 60;
  mins = mins % 60;

  if (hours > 0) {
    snprintf(buf, buf_size, "%luh %lum", (unsigned long)hours, (unsigned long)mins);
  } else {
    snprintf(buf, buf_size, "%lum", (unsigned long)mins);
  }
}

static void prv_content_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  OfflineSummary *summary = game_state_get_offline_summary();

  // Background
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Border card
  GRect card_rect = GRect(6, 6, bounds.size.w - 12, bounds.size.h - 12);
  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
  graphics_draw_rect(ctx, card_rect);

  // Header Title
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "WHILE AWAY",
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 10, bounds.size.w, 28),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  char away_str[24];
  char credited_str[24];
  prv_format_duration(summary->away_sec, away_str, sizeof(away_str));
  prv_format_duration(summary->credited_sec, credited_str, sizeof(credited_str));

  char body_buf[160];
  uint32_t eff_pct = game_state_get_offline_speed_percent();
  if (!game_state_is_offline_unlocked()) {
    snprintf(body_buf, sizeof(body_buf),
             "Away: %s\nCredited: 0m\n\nIdle time locked!\nUnlock in Store (1¢)\n\n+0 Pixels",
             away_str);
  } else {
    if (summary->fills_gained > 0) {
      snprintf(body_buf, sizeof(body_buf),
               "Away: %s (%lu%% speed)\nCredited: %s\n\n+%lu Pixels\n+%lu Full Fills!",
               away_str, (unsigned long)eff_pct, credited_str,
               (unsigned long)summary->pixels_gained,
               (unsigned long)summary->fills_gained);
    } else {
      snprintf(body_buf, sizeof(body_buf),
               "Away: %s (%lu%% speed)\nCredited: %s\n\n+%lu Pixels Filled",
               away_str, (unsigned long)eff_pct, credited_str,
               (unsigned long)summary->pixels_gained);
    }
  }

  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, body_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(12, 40, bounds.size.w - 24, bounds.size.h - 70),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);

  // Dismiss Prompt
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "[SELECT to Play]",
                     fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, bounds.size.h - 26, bounds.size.w, 18),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_dismiss_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_summary_window) {
    window_stack_pop(true);
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_dismiss_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_dismiss_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_content_layer = layer_create(bounds);
  layer_set_update_proc(s_content_layer, prv_content_update_proc);
  layer_add_child(window_layer, s_content_layer);

  GameState *game = game_state_get();
  if (game->data.option_vibration) {
    vibes_short_pulse();
  }
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_content_layer);
  s_content_layer = NULL;
  window_destroy(s_summary_window);
  s_summary_window = NULL;
}

void ui_offline_summary_show_if_needed(void) {
  OfflineSummary *summary = game_state_get_offline_summary();
  if (!summary->should_show) {
    return;
  }
  // Clear the flag so it only shows once upon launch
  summary->should_show = false;

  s_summary_window = window_create();
  window_set_click_config_provider(s_summary_window, prv_click_config_provider);
  window_set_window_handlers(s_summary_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_summary_window, true);
}
