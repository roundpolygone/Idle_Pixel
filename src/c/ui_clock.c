#include "ui_clock.h"
#include "game_state.h"

static Window *s_clock_window = NULL;
static Layer *s_clock_layer = NULL;

static void prv_clock_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GameState *game = game_state_get();

  GColor bg_color = game_state_get_bg_color();
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  bool is_dark = game_state_is_color_dark(bg_color);
  GColor fg_color = is_dark ? GColorWhite : GColorBlack;
  GColor accent_color = is_dark ? GColorCyan : GColorCobaltBlue;

  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  if (!game->data.option_analog_clock) {
    // ---------------------------------------------------------
    // Digital Clock (Big Digits default)
    // ---------------------------------------------------------
    char time_buf[16];
    if (game->data.option_24h) {
      snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tick_time->tm_hour, tick_time->tm_min);
    } else {
      int hour = tick_time->tm_hour % 12;
      if (hour == 0) hour = 12;
      snprintf(time_buf, sizeof(time_buf), "%d:%02d", hour, tick_time->tm_min);
    }

    int digit_y = (bounds.size.h / 2) - 38;
    graphics_context_set_text_color(ctx, fg_color);
    GFont time_font = game_state_get_font(game->data.selected_font, true);
    graphics_draw_text(ctx, time_buf,
                       time_font,
                       GRect(0, digit_y, bounds.size.w, 48),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);

    // Date
    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%a, %b %e", tick_time);
    graphics_draw_text(ctx, date_buf,
                       fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, digit_y + 46, bounds.size.w, 24),
                       GTextOverflowModeWordWrap,
                       GTextAlignmentCenter,
                       NULL);
  } else {
    // ---------------------------------------------------------
    // Analog Clock
    // ---------------------------------------------------------
    GPoint center = grect_center_point(&bounds);
    int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 - 20;

    // Dial ring
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_draw_circle(ctx, center, radius);

    // 12 hour tick marks
    for (int i = 0; i < 12; i++) {
      int32_t angle = (TRIG_MAX_ANGLE * i) / 12;
      GPoint p1 = (GPoint) {
        .x = (int16_t)(sin_lookup(angle) * (radius - 5) / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(angle) * (radius - 5) / TRIG_MAX_RATIO) + center.y,
      };
      GPoint p2 = (GPoint) {
        .x = (int16_t)(sin_lookup(angle) * radius / TRIG_MAX_RATIO) + center.x,
        .y = (int16_t)(-cos_lookup(angle) * radius / TRIG_MAX_RATIO) + center.y,
      };
      graphics_draw_line(ctx, p1, p2);
    }

    // Hour Hand
    int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6) + (tick_time->tm_min / 10))) / (12 * 6);
    int16_t hour_len = radius * 5 / 10;
    GPoint hour_pt = (GPoint) {
      .x = (int16_t)(sin_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(hour_angle) * hour_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, fg_color);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, center, hour_pt);

    // Minute Hand
    int32_t min_angle = (TRIG_MAX_ANGLE * tick_time->tm_min) / 60;
    int16_t min_len = radius * 8 / 10;
    GPoint min_pt = (GPoint) {
      .x = (int16_t)(sin_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(min_angle) * min_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, center, min_pt);

    // Second Hand
    int32_t sec_angle = (TRIG_MAX_ANGLE * tick_time->tm_sec) / 60;
    int16_t sec_len = radius * 9 / 10;
    GPoint sec_pt = (GPoint) {
      .x = (int16_t)(sin_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(sec_angle) * sec_len / TRIG_MAX_RATIO) + center.y,
    };
    graphics_context_set_stroke_color(ctx, accent_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, center, sec_pt);

    // Center pin
    graphics_context_set_fill_color(ctx, fg_color);
    graphics_fill_circle(ctx, center, 3);
  }

  // Mini Idle Pixel stats at bottom
  char idle_buf[48];
  snprintf(idle_buf, sizeof(idle_buf), "PX %lu/64 | COL %u/64 | GP %lu",
           (unsigned long)game->data.current_pixels,
           (unsigned int)(game->data.current_color_idx + 1),
           (unsigned long)game->data.gold_pixels);
  graphics_context_set_text_color(ctx, fg_color);
  graphics_draw_text(ctx, idle_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(0, bounds.size.h - 18, bounds.size.w, 16),
                     GTextOverflowModeWordWrap,
                     GTextAlignmentCenter,
                     NULL);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_clock_layer) {
    layer_mark_dirty(s_clock_layer);
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_clock_layer = layer_create(bounds);
  layer_set_update_proc(s_clock_layer, prv_clock_update_proc);
  layer_add_child(window_layer, s_clock_layer);

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
}

static void prv_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  layer_destroy(s_clock_layer);
  s_clock_layer = NULL;
  window_destroy(s_clock_window);
  s_clock_window = NULL;
}

void ui_clock_show(void) {
  if (s_clock_window) {
    return;
  }
  s_clock_window = window_create();
  window_set_window_handlers(s_clock_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_clock_window, true);
}
